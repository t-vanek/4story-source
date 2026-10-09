#!/usr/bin/env python3
"""Verify the Linux image's six daemons, health endpoints and SIGTERM exit."""

import argparse
import json
from pathlib import Path
import subprocess
import time
import uuid


SERVICES = (
    ("world", "tworldsvr", 18087),
    ("log", "tlogsvr", 8800),
    ("control", "tcontrolsvr", 18086),
    ("login", "tloginsvr", 8815),
    ("patch", "tpatchsvr", 8915),
    ("map", "tmapsvr", 8916),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", choices=("docker", "podman"), default="docker")
    parser.add_argument("--image", default="fourstory:linux")
    parser.add_argument("--binary-directory", type=Path,
                        help="Verify locally built binaries mounted read-only in the runtime image")
    args = parser.parse_args()
    binary_mount = []
    if args.binary_directory:
        binary_dir = args.binary_directory.resolve(strict=True)
        for _, binary, _ in SERVICES:
            if not (binary_dir / f"{binary}_asio").is_file():
                parser.error(f"Missing locally built daemon: {binary}_asio")
        binary_mount = ["-v", f"{binary_dir}:/opt/verification-bin:ro,z"]
    config = Path(__file__).resolve().parents[1] / "deploy/config.example"
    network = f"fourstory-smoke-{uuid.uuid4().hex[:10]}"
    containers = []

    def run(*command, check=True):
        result = subprocess.run(
            [args.engine, *command], capture_output=True,
            text=True, timeout=45,
        )
        if check and result.returncode != 0:
            raise RuntimeError(
                f"{args.engine} {' '.join(command)}: {result.stderr or result.stdout}"
            )
        return result

    try:
        run("network", "create", network)
        # Verify the SQL driver and OpenSSL RC4 provider in the final image.
        run("run", "--rm", args.image, "odbcinst", "-q", "-d",
            "-n", "ODBC Driver 18 for SQL Server")
        run("run", "--rm", args.image, "openssl", "enc", "-rc4",
            "-provider", "default", "-provider", "legacy",
            "-K", "00000000000000000000000000000000",
            "-in", "/dev/null", "-out", "/dev/null")
        for alias, binary, port in SERVICES:
            name = f"{network}-{alias}"
            containers.append(name)
            run("run", "-d", "--name", name, "--network", network,
                "--network-alias", alias, "--read-only", "--tmpfs", "/tmp",
                "--cap-drop", "ALL", "--security-opt", "no-new-privileges",
                "-v", f"{config}:/etc/fourstory:ro,z", *binary_mount, args.image,
                (f"/opt/verification-bin/{binary}_asio" if binary_mount else f"{binary}_asio"),
                "--config", f"/etc/fourstory/{binary}.toml")
            deadline = time.monotonic() + 30
            while True:
                probe = run("exec", name, "curl", "-fsS", "--max-time", "2",
                            f"http://127.0.0.1:{port}/healthz", check=False)
                if probe.returncode == 0 and json.loads(probe.stdout)["status"] == "ok":
                    break
                state = json.loads(run("inspect", name).stdout)[0]["State"]
                if not state["Running"]:
                    raise RuntimeError(f"{alias}: exited before becoming healthy")
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"{alias}: health endpoint did not become ready")
                time.sleep(0.5)
            print(f"{alias}: /healthz OK", flush=True)

        deadline = time.monotonic() + 15
        while "world_client: connected to world:3815" not in run("logs", containers[-1]).stdout:
            if time.monotonic() >= deadline:
                raise RuntimeError("map did not connect to world over container DNS")
            time.sleep(0.5)
        print("map -> world: connected over container DNS", flush=True)

        # Explicit SIGTERM must produce a clean exit without forced SIGKILL.
        for name in reversed(containers):
            run("kill", "--signal", "TERM", name)
            deadline = time.monotonic() + 15
            while True:
                state = json.loads(run("inspect", name).stdout)[0]["State"]
                if not state["Running"]:
                    if state["ExitCode"] != 0:
                        raise RuntimeError(f"{name}: exit code {state['ExitCode']}")
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"{name}: did not exit after SIGTERM")
                time.sleep(0.5)
            print(f"{name.rsplit('-', 1)[-1]}: SIGTERM exit 0", flush=True)
    except (RuntimeError, subprocess.SubprocessError, ValueError, KeyError):
        for name in containers:
            logs = run("logs", name, check=False)
            print(f"--- {name} ---\n{logs.stdout}{logs.stderr}", flush=True)
        raise
    finally:
        for name in reversed(containers):
            run("rm", "-f", name, check=False)
        run("network", "rm", network, check=False)


if __name__ == "__main__":
    main()
