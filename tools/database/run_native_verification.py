#!/usr/bin/env python3
"""Verify real native C++ PostgreSQL pools/catalogs and map daemon in an owned lab.

Use disposable_environment.py start --postgresql-only first. Requires the existing
private 15-table reference snapshot and a compiled Linux build. All created data,
roles and certificates belong to the disposable PostgreSQL instance. No SQL Server
is used. Logs are scrubbed; credentials and extracted source rows stay outside Git.
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time
import urllib.request
import uuid

from activate_catalog import activate_catalog
from disposable_environment import command, private_file, verify_container, LABEL
from migrate import apply_migrations, import_snapshot


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--snapshot', type=Path, help='Private reference manifest.json')
    p.add_argument('--login-only', action='store_true', help='Synthetic native Login integration and actual-daemon protocol tests')
    p.add_argument('--characters-only', action='store_true', help='Native character lifecycle with backup-derived charts')
    p.add_argument('--map-backend-only', action='store_true', help='With --map-runtime-only, stop after native backend integration tests')
    p.add_argument('--map-runtime-only', action='store_true', help='Native Map claim/load/save with source catalogs')
    p.add_argument('--actor-snapshot', type=Path, help='Private four-table actor manifest')
    p.add_argument('--routing-only', action='store_true', help='Native authenticated Login-to-Map routing and handoff')
    p.add_argument('--routing-snapshot', type=Path, help='Private four-table routing manifest')
    p.add_argument('--pool-only', action='store_true', help='Synthetic pool/TLS tests for CI; no historical data or map startup')
    p.add_argument('--build-dir', default='build/linux-debug')
    p.add_argument('--image', default='localhost/fourstory:build-deps')
    p.add_argument('--server-binary', help='Installed map binary to test instead of the mounted Debug executable')
    p.add_argument('--runtime-bin-dir', help='Installed Login/World/Map directory for --map-runtime-only; integration tests still use --build-dir')
    p.add_argument('--report', type=Path, default=Path('_rewrite/docs/modernization/evidence/native-postgresql.json'))
    args = p.parse_args()
    login_mode = args.login_only or args.characters_only or args.routing_only or args.map_runtime_only
    character_mode = args.characters_only or args.routing_only or args.map_runtime_only
    if (args.routing_only or args.map_runtime_only) and args.routing_snapshot is None:
        p.error("--routing-snapshot is required for --routing-only")
    if sum((args.pool_only, args.login_only, args.characters_only, args.routing_only, args.map_runtime_only)) > 1:
        p.error("Choose one synthetic mode")
    if not (args.pool_only or args.login_only) and args.snapshot is None:
        p.error('--snapshot is required unless a synthetic mode is selected')
    if args.map_runtime_only and args.actor_snapshot is None:
        p.error("--actor-snapshot is required for --map-runtime-only")
    if args.runtime_bin_dir and (not args.map_runtime_only or not args.runtime_bin_dir.startswith('/')):
        p.error('--runtime-bin-dir requires --map-runtime-only and an absolute container path')
    import psycopg
    from psycopg import sql
    repo = Path(__file__).resolve().parents[2]
    work = args.work.resolve()
    if work.is_relative_to(repo):
        raise RuntimeError('Private native verification directory must be outside repository')
    # TLS server certificate/key belong to the whole disposable PostgreSQL
    # instance, not to an individual database. Independent DB names alone do
    # not make simultaneous verification against one work directory safe.
    import fcntl
    verification_lock = (work / '.native-verification.lock').open('a')
    try:
        fcntl.flock(verification_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        p.error('Another native verification owns this lab; use a different --work or wait for it to finish')
    state = json.loads((work / 'state.json').read_text())
    verify_container(state, state['pg_container'])
    run = uuid.uuid4().hex[:12]
    private = work / ('native-' + run)
    private.mkdir(mode=0o700)
    tls = private / 'tls'
    tls.mkdir(mode=0o700)
    public = private / 'client-files'
    public.mkdir(mode=0o755)
    (public / 'tls').mkdir(mode=0o755)
    values = dict(line.split('=', 1) for line in (work / 'postgres.env').read_text().splitlines())
    admin = dict(host='127.0.0.1', port=state['pg_port'], user='postgres',
                 password=values['POSTGRES_PASSWORD'], autocommit=True)
    dbname = 'fourstory_runtime_' + run
    app_role, reader_role = 'native_app_' + run, 'native_reader_' + run
    app_password, reader_password = secrets.token_urlsafe(30), secrets.token_urlsafe(30)
    private_values = [admin['password'], app_password, reader_password]
    result = {'scope': ('Synthetic native SOCI/PostgreSQL pool and TLS verification' if args.pool_only else
                       'Native SOCI/PostgreSQL + real map catalog startup; original-client testing PENDING'),
              'sql_server_used': False, 'tests': []}
    if login_mode:
        result['scope'] = 'Native PostgreSQL Login with synthetic accounts and source-derived encrypted packets; original-client test PENDING'
    # Resolve a mutable tag once. Every integration executable and daemon in
    # this run must use the same immutable image, even during another build.
    image_reference = args.image
    args.image = json.loads(command(['image', 'inspect', image_reference]))[0]['Id']
    result['image'] = {'reference': image_reference, 'id': args.image}
    server_name = ('fourstory-native-login-' if login_mode else 'fourstory-native-map-') + run

    smtp_name = 'fourstory-native-smtp-' + run
    spool = private / 'mail-spool'

    def scrub(text):
        for value in private_values:
            text = text.replace(value, '[REDACTED]')
        return text

    def execute(argv, **kwargs):
        completed = subprocess.run(argv, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
        if completed.returncode:
            raise RuntimeError('Command failed: ' + scrub(completed.stdout[-5000:]))
        return completed.stdout

    def reject_sanitizer_errors(output):
        if any(marker in output for marker in ('ERROR: AddressSanitizer', 'ERROR: LeakSanitizer', 'runtime error:')):
            raise RuntimeError('Daemon sanitizer failure: ' + scrub(output[-12000:]))

    try:
        # One-use CA, server identity and deliberately untrusted CA for TLS tests.
        for prefix in ('ca', 'wrong-ca'):
            execute(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-sha256', '-days', '1',
                     '-keyout', str(tls / (prefix + '.key')), '-out', str(tls / (prefix + '.crt')),
                     '-subj', '/CN=4Story-Disposable-' + prefix])
        execute(['openssl', 'req', '-newkey', 'rsa:2048', '-nodes', '-keyout', str(tls / 'server.key'),
                 '-out', str(tls / 'server.csr'), '-subj', '/CN=' + state['pg_container']])
        (tls / 'server.ext').write_text('subjectAltName=DNS:' + state['pg_container'] + '\nextendedKeyUsage=serverAuth\n')
        execute(['openssl', 'x509', '-req', '-in', str(tls / 'server.csr'), '-CA', str(tls / 'ca.crt'),
                 '-CAkey', str(tls / 'ca.key'), '-CAcreateserial', '-days', '1', '-sha256',
                 '-extfile', str(tls / 'server.ext'), '-out', str(tls / 'server.crt')])
        for key in tls.glob('*.key'):
            key.chmod(0o600)
        for certificate in ('ca.crt', 'wrong-ca.crt'):
            shutil.copyfile(tls / certificate, public / 'tls' / certificate)
            (public / 'tls' / certificate).chmod(0o644)
        command(['exec', state['pg_container'], 'mkdir', '-p', '/tmp/fourstory-native-tls'])
        for name in ('server.key', 'server.crt'):
            command(['cp', str(tls / name), state['pg_container'] + ':/tmp/fourstory-native-tls/' + name])
        command(['exec', state['pg_container'], 'chown', '-R', 'postgres:postgres', '/tmp/fourstory-native-tls'])
        command(['exec', state['pg_container'], 'chmod', '600', '/tmp/fourstory-native-tls/server.key'])
        with psycopg.connect(dbname='postgres', **admin) as conn:
            conn.execute("ALTER SYSTEM SET ssl_cert_file = '/tmp/fourstory-native-tls/server.crt'")
            conn.execute("ALTER SYSTEM SET ssl_key_file = '/tmp/fourstory-native-tls/server.key'")
            conn.execute("ALTER SYSTEM SET ssl = 'on'")
            conn.execute('SELECT pg_reload_conf()')
            conn.execute(sql.SQL('CREATE DATABASE {}').format(sql.Identifier(dbname)))
        with psycopg.connect(dbname=dbname, **admin) as conn:
            result['engine'] = conn.execute('SELECT version()').fetchone()[0]
            if not args.pool_only:
                result['migrations'] = apply_migrations(conn, repo / 'database/postgresql')
                if not login_mode or character_mode:
                    result['import'] = import_snapshot(conn, args.snapshot, repo / 'database/postgresql/mapping.json')
                    if character_mode:
                        from activate_character_catalog import activate_character_catalog
                        result['activation'] = activate_character_catalog(conn, args.snapshot)
                    else:
                        result['activation'] = activate_catalog(conn, args.snapshot)
                if args.routing_only or args.map_runtime_only:
                    from activate_routing_catalog import activate_routing_catalog
                    result['routing_import'] = import_snapshot(conn, args.routing_snapshot, repo / 'database/postgresql/mapping.json')
                    result['routing_activation'] = activate_routing_catalog(conn, args.routing_snapshot)
                    from verify_routing_publication import verify_routing_publication
                    result['routing_publication'] = verify_routing_publication(conn, args.routing_snapshot, private)
                if args.map_runtime_only:
                    from activate_actor_catalog import activate_actor_catalog
                    result['actor_import'] = import_snapshot(conn, args.actor_snapshot, repo / 'database/postgresql/mapping.json')
                    result['actor_activation'] = activate_actor_catalog(conn, args.actor_snapshot)
            else:
                # Permission test target exists, but contains no invented source rows.
                conn.execute('CREATE SCHEMA legacy_game')
                conn.execute('CREATE TABLE legacy_game."TMONSTERCHART"(synthetic_marker integer)')
            for role, password in ((app_role, app_password), (reader_role, reader_password)):
                conn.execute(sql.SQL('CREATE ROLE {} LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE PASSWORD {}').format(
                    sql.Identifier(role), sql.Literal(password)))
            conn.execute(sql.SQL('REVOKE ALL ON DATABASE {} FROM PUBLIC').format(sql.Identifier(dbname)))
            conn.execute(sql.SQL('GRANT CONNECT ON DATABASE {} TO {}, {}').format(
                sql.Identifier(dbname), sql.Identifier(app_role), sql.Identifier(reader_role)))
            conn.execute('CREATE SCHEMA synthetic_runtime')
            conn.execute('CREATE TABLE synthetic_runtime.transactions(id integer PRIMARY KEY, value text NOT NULL)')
            conn.execute('CREATE TABLE synthetic_runtime.counters(id integer PRIMARY KEY, value integer NOT NULL)')
            conn.execute('INSERT INTO synthetic_runtime.counters VALUES (1,0)')
            conn.execute(sql.SQL('GRANT USAGE ON SCHEMA synthetic_runtime TO {}').format(sql.Identifier(app_role)))
            conn.execute(sql.SQL('GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA synthetic_runtime TO {}').format(sql.Identifier(app_role)))
            if not (args.pool_only or login_mode):
                conn.execute(sql.SQL('GRANT USAGE ON SCHEMA game_compat TO {}').format(sql.Identifier(reader_role)))
                conn.execute(sql.SQL('GRANT SELECT ON ALL TABLES IN SCHEMA game_compat TO {}').format(sql.Identifier(reader_role)))

            if login_mode:
                grants = (repo / 'deploy/sql/login-runtime-grants.sql').read_text()
                # Same checked-in deployment grants; substitute only the psql
                # identifier parameter using psycopg's SQL identifier quoting.
                conn.execute(grants.replace(':"login_role"', sql.Identifier(app_role).as_string(conn)))
                if character_mode:
                    grants = (repo / 'deploy/sql/character-runtime-grants.sql').read_text()
                    conn.execute(grants.replace(':"login_role"', sql.Identifier(app_role).as_string(conn)))
                if args.routing_only or args.map_runtime_only:
                    grants = (repo / 'deploy/sql/routing-runtime-grants.sql').read_text()
                    conn.execute(grants.replace(':"login_role"', sql.Identifier(app_role).as_string(conn)))

            if args.map_runtime_only:
                grants=(repo / 'deploy/sql/map-runtime-grants.sql').read_text()
                conn.execute(grants.replace(':"map_role"', sql.Identifier(reader_role).as_string(conn)))

        deadline = time.monotonic() + 10
        while True:
            with psycopg.connect(dbname=dbname, **admin) as conn:
                if conn.execute('SHOW ssl').fetchone()[0] == 'on':
                    break
            if time.monotonic() > deadline:
                raise RuntimeError('PostgreSQL TLS reload timed out')
            time.sleep(0.1)

        # Omit sslmode deliberately: the native pool must default to verify-full.
        base = f"host={state['pg_container']} port=5432 dbname={dbname} connect_timeout=3 sslrootcert=/run/fourstory-test/tls/ca.crt"
        app_conn = base + f' user={app_role} password={app_password}'
        reader_conn = base + f' user={reader_role} password={reader_password}'
        manifest = result.get('activation', {}).get('manifest_sha256', '')
        private_file(private / 'native.env', '\n'.join([
            'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1',
            'UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1',
            'FOURSTORY_TEST_PG_CONNINFO=' + app_conn,
            'FOURSTORY_TEST_PG_UNTRUSTED_CONNINFO=' + app_conn.replace('/tls/ca.crt', '/tls/wrong-ca.crt'),
            'FOURSTORY_CATALOG_PG_CONNINFO=' + reader_conn,
            'FOURSTORY_CATALOG_MANIFEST=' + manifest,
            'FOURSTORY_ROUTING_MANIFEST=' + result.get('routing_activation',{}).get('manifest_sha256',''),
            'FOURSTORY_CONTENT_CONNECTION=' + reader_conn,
            'FOURSTORY_MAP_PG_CONNINFO=' + reader_conn,
            'FOURSTORY_ACTOR_MANIFEST=' + result.get('actor_activation',{}).get('manifest_sha256',''),
            *(['FOURSTORY_LOGIN_FIXTURE_CONNINFO=' + base + ' user=postgres password=' + admin['password']] if login_mode else []), '']))
        base_run = ['podman', 'run', '--rm', '--network', state['network'],
                    '-v', str(repo) + ':/src:ro,z', '-v', str(public) + ':/run/fourstory-test:ro,z',
                    '--env-file', str(private / 'native.env'), args.image]
        bin_dir = '/src/' + args.build_dir.rstrip('/') + '/bin/'
        server_binary = args.server_binary or bin_dir + ('tloginsvr_asio' if login_mode else 'tmapsvr_asio')
        checks = [('test_fourstory_postgresql', ['--options-only']), ('test_fourstory_postgresql', [])]
        if login_mode:
            checks.append(('test_tmapsvr_postgresql_map' if args.map_runtime_only else
                           'test_tloginsvr_postgresql_handoff' if args.routing_only else
                           'test_tloginsvr_postgresql_char' if args.characters_only else 'test_tloginsvr_postgresql_auth', []))
        elif not args.pool_only:
            checks.append(('test_tmapsvr_postgresql_catalog', []))
        for binary, extra in checks:
            output = execute(base_run + [bin_dir + binary, *extra], timeout=120)
            result['tests'].append({'binary': binary, 'arguments': extra, 'exit_code': 0, 'log': scrub(output)})
            print(binary + (' options' if extra else '') + ': passed', flush=True)

        if args.map_backend_only and args.map_runtime_only:
            result['status']='passed'
            result['scope']='Native PostgreSQL backend integration only; daemon protocol verification excluded'
            return

        if args.pool_only:
            result['status'] = 'passed'
            return

        if args.map_runtime_only:
            from verify_map_runtime_wire import verify_map_runtime_daemons
            runtime_bin_dir=args.runtime_bin_dir.rstrip('/')+'/' if args.runtime_bin_dir else bin_dir
            result['daemon_binaries']={'image':args.image,'directory':runtime_bin_dir,
                                       'integration_test_directory':bin_dir}
            with psycopg.connect(dbname=dbname, **admin) as conn:
                result['map_wire']=verify_map_runtime_daemons(conn,state,repo,private,public,app_conn,reader_conn,args.image,runtime_bin_dir,server_name,
                    manifest,result['routing_activation']['manifest_sha256'],result['actor_activation']['manifest_sha256'],execute)
            result['scope']='Native PostgreSQL primary/replica admission, core checkpoint/save/crash recovery; replica tests use an explicitly synthetic cell partition and restore the source view; original client PENDING'
            result['status']='passed'
            return

        if args.routing_only:
            from verify_routing_wire import verify_routing_daemon
            with psycopg.connect(dbname=dbname, **admin) as conn:
                result['routing_wire'] = verify_routing_daemon(
                    conn, state, repo, private, public, app_conn, args.image,
                    server_binary, server_name, manifest, result['routing_activation']['manifest_sha256'], execute)
            result['scope'] = 'Native PostgreSQL Login START handoff using original routing charts; native Map claim/load/save and original client PENDING'
            result['status'] = 'passed'
            return

        if args.characters_only:
            from verify_character_wire import verify_character_daemon
            with psycopg.connect(dbname=dbname, **admin) as conn:
                result['character_wire'] = verify_character_daemon(
                    conn, state, repo, private, public, app_conn, args.image,
                    server_binary, server_name, manifest, execute)
            result['scope'] = 'Native PostgreSQL characters with backup charts and source-derived TCP; original client PENDING'
            result['status'] = 'passed'
            return

        if login_mode:
            # Never pass the fixture administrator credential to the daemon.
            private_file(private / 'server.env', 'FOURSTORY_TEST_PG_CONNINFO=' + app_conn + '\nASAN_OPTIONS=detect_leaks=1:halt_on_error=1\nUBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1\n')
            # Local-only SMTP fixture; no host port and no outbound forwarding.
            spool.mkdir(mode=0o700)
            command(['run', '-d', '--name', smtp_name, '--label', LABEL + '=' + state['token'],
                     '--network', state['network'], '-v', str(repo) + ':/src:ro,z',
                     '-v', str(spool) + ':/spool:rw,z', 'docker.io/library/node:24-alpine',
                     'node', '/src/tools/database/smtp_fixture.mjs', '/spool'])
            result['smtp_fixture_image'] = json.loads(command(['inspect', smtp_name]))[0]['Image']
            deadline = time.monotonic() + 15
            while not (spool / 'ready').exists():
                if time.monotonic() > deadline: raise RuntimeError('Private SMTP fixture readiness timeout')
                time.sleep(.05)
            config = ('[server]\nport=4816\ncontrol_server_ip="127.0.0.1"\n[database]\nbackend="postgresql"\n'
                      'connection_string_env="FOURSTORY_TEST_PG_CONNINFO"\npool_size=4\nworker_threads=2\n'
                      f'[smtp]\nhost="{smtp_name}"\nport=2525\nfrom_address="verification@example.invalid"\n'
                      '[security]\ndb_trust_store=false\n[health]\nport=18916\n[metrics]\nport=0\n[shutdown]\ndrain_ms=50\n[log]\nlevel="info"\n')
            (public / 'login.toml').write_text(config)
            (public / 'login.toml').chmod(0o644)
            daemon_run = ['podman', 'run', '--rm', '--network', state['network'],
                          '-v', str(repo) + ':/src:ro,z', '-v', str(public) + ':/run/fourstory-test:ro,z',
                          '--env-file', str(private / 'server.env'), args.image, server_binary]
            (public / 'missing-database.toml').write_text('[server]\ncontrol_server_ip="127.0.0.1"\n')
            missing = subprocess.run(daemon_run + ['--config', '/run/fourstory-test/missing-database.toml'],
                                     text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
            reject_sanitizer_errors(missing.stdout)
            if missing.returncode == 0 or 'Login requires database configuration' not in missing.stdout or 'server listening' in missing.stdout:
                raise RuntimeError('Missing auth configuration was not refused before listening')
            result['missing_database_refused'] = True

            command(['run', '-d', '--name', server_name, '--label', LABEL + '=' + state['token'],
                     '--network', state['network'], '-p', '127.0.0.1::18916', '-p', '127.0.0.1::4816',
                     '-v', str(repo) + ':/src:ro,z', '-v', str(public) + ':/run/fourstory-test:ro,z',
                     '--env-file', str(private / 'server.env'), args.image,
                     server_binary, '--config', '/run/fourstory-test/login.toml'])
            port = command(['port', server_name, '18916/tcp']).rsplit(':', 1)[1]
            deadline = time.monotonic() + 30
            while True:
                try:
                    with urllib.request.urlopen('http://127.0.0.1:' + port + '/healthz', timeout=1) as response:
                        if response.status == 200: break
                except (OSError, TimeoutError): pass
                if not json.loads(command(['inspect', server_name]))[0]['State']['Running'] or time.monotonic() > deadline:
                    raise RuntimeError('Login startup failed: ' + scrub(execute(['podman', 'logs', server_name])))
                time.sleep(0.1)
            from verify_login_wire import verify_login_wire
            login_port = int(command(['port', server_name, '4816/tcp']).rsplit(':', 1)[1])
            try:
                result['wire'] = verify_login_wire('127.0.0.1', login_port)
            except Exception:
                result['login_failure_log'] = scrub(execute(['podman', 'logs', server_name]))
                raise
            with psycopg.connect(dbname=dbname, **admin) as conn:
                deadline = time.monotonic() + 5
                while conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=201').fetchone()[0]:
                    if time.monotonic() > deadline: raise RuntimeError('Wire disconnect left current session')
                    time.sleep(0.05)
                result['wire_audit_rows'] = conn.execute('SELECT count(*) FROM app_global."TLOG" WHERE "dwUserID"=201').fetchone()[0]
                if result['wire_audit_rows'] != 2:
                    raise RuntimeError('Wire duplicate produced unexpected audit count')
                while True:
                    audit_count = conn.execute('SELECT count(*) FROM app_global."TLOG" WHERE "dwUserID"=203').fetchone()[0]
                    current_count = conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=203').fetchone()[0]
                    if audit_count == 1 and current_count == 0: break
                    if time.monotonic() > deadline:
                        raise RuntimeError('Abandoned login did not both commit audit and remove current session')
                    time.sleep(0.05)
                result['disconnect_during_auth'] = 'committed audit retained, current session cleaned'

            from verify_security_wire import verify_security_wire
            with psycopg.connect(dbname=dbname, **admin) as conn:
                result['security_wire'] = verify_security_wire('127.0.0.1', login_port, spool, conn)
            print('tloginsvr_asio: connection-bound security wire flow passed', flush=True)

            import socket
            from verify_login_wire import frame, login_body
            from verify_security_wire import CREDENTIAL as SECURITY_CREDENTIAL
            time.sleep(10.1)
            with socket.create_connection(('127.0.0.1', login_port), timeout=20) as stalled:
                stalled.sendall(frame(login_body(b'SyntheticSecurity221', SECURITY_CREDENTIAL)))
                deadline = time.monotonic() + 5
                while not (spool / 'stalled').exists():
                    if time.monotonic() > deadline: raise RuntimeError('SMTP stall fixture not reached')
                    time.sleep(.025)
                with urllib.request.urlopen('http://127.0.0.1:' + port + '/healthz', timeout=.5) as response:
                    if response.status != 200: raise RuntimeError('SMTP blocked reactor health')
                started = time.monotonic()
                if stalled.recv(1) != b'': raise RuntimeError('Stalled SMTP created unexpected game reply')
                elapsed = time.monotonic() - started
                if elapsed > 17: raise RuntimeError('SMTP deadline exceeded')
            with psycopg.connect(dbname=dbname, **admin) as conn:
                deadline = time.monotonic() + 5
                while conn.execute('SELECT count(*) FROM app_global.login_security_challenge WHERE user_id=221').fetchone()[0]:
                    if time.monotonic() > deadline: raise RuntimeError('Failed SMTP left pending challenge')
                    time.sleep(.025)
                if conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=221').fetchone()[0]:
                    raise RuntimeError('Failed SMTP created a session')
            result['smtp_stall'] = {'health_responsive': True, 'deadline_seconds': 15,
                                    'connection_closed': True, 'challenge_removed': True}
            print('tloginsvr_asio: SMTP deadline and reactor health passed', flush=True)

            # Hold a real socket to prove a refused second process cannot
            # sweep the first owner's sessions during its startup.
            import socket
            from verify_login_wire import frame, login_body, ack
            time.sleep(10.1)  # actual default per-IP limiter, no test bypass
            with socket.create_connection(('127.0.0.1', login_port), timeout=5) as live:
                live.sendall(frame(login_body(b'SyntheticLive')))
                if ack(live)[0] != 0: raise RuntimeError('Live-owner fixture login failed')
                with psycopg.connect(dbname=dbname, **admin) as conn:
                    owner_pid = conn.execute('SELECT backend_pid FROM app_global.login_runtime_owner').fetchone()[0]
                    second = subprocess.run(daemon_run + ['--config', '/run/fourstory-test/login.toml'],
                                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
                    reject_sanitizer_errors(second.stdout)
                    if second.returncode == 0 or 'Another native Login owner is active' not in second.stdout or 'server listening' in second.stdout:
                        raise RuntimeError('Second Login owner was not refused before session recovery/listening')
                    if conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=204').fetchone()[0] != 1:
                        raise RuntimeError('Rejected Login startup altered active owner session')
                    result['second_owner_refused'] = {'exit_code': second.returncode, 'active_session_preserved': True}
                    conn.execute('SELECT pg_terminate_backend(%s)', (owner_pid,))
                lost_exit = execute(['podman', 'wait', server_name], timeout=15).strip()
                loss_logs = execute(['podman', 'logs', server_name])
                reject_sanitizer_errors(loss_logs)
                if lost_exit != '1' or 'Native Login ownership lost; shutting down' not in loss_logs:
                    raise RuntimeError('Owner-connection loss did not stop the daemon with failure status')
                result['ownership_loss'] = {'exit_code': 1, 'log': scrub(loss_logs)}

            # Restart the same owned container; it must acquire a fresh process
            # owner and recover through the real main/config path.
            command(['start', server_name])
            port = command(['port', server_name, '18916/tcp']).rsplit(':', 1)[1]
            login_port = int(command(['port', server_name, '4816/tcp']).rsplit(':', 1)[1])
            deadline = time.monotonic() + 30
            while True:
                try:
                    with urllib.request.urlopen('http://127.0.0.1:' + port + '/healthz', timeout=1) as response:
                        if response.status == 200: break
                except (OSError, TimeoutError): pass
                if time.monotonic() > deadline: raise RuntimeError('Replacement owner did not become healthy')
                time.sleep(0.1)
            with psycopg.connect(dbname=dbname, **admin) as conn:
                new_pid = conn.execute('SELECT backend_pid FROM app_global.login_runtime_owner').fetchone()[0]
                if new_pid == owner_pid: raise RuntimeError('Replacement did not acquire a new owner connection')
            result['replacement_started'] = True

            with socket.create_connection(('127.0.0.1', login_port), timeout=5) as draining:
                draining.sendall(frame(login_body(b'SyntheticDrain')))
                with psycopg.connect(dbname=dbname, **admin) as conn:
                    deadline = time.monotonic() + 5
                    while not conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]:
                        if time.monotonic() > deadline: raise RuntimeError('Shutdown fixture never reached its delayed DB transaction')
                        time.sleep(0.01)
                # Health must remain responsive while a DB worker is occupied.
                with urllib.request.urlopen('http://127.0.0.1:' + port + '/healthz', timeout=0.5) as response:
                    if response.status != 200: raise RuntimeError('DB operation blocked reactor health')
                command(['kill', '--signal', 'TERM', server_name])
                exit_code = execute(['podman', 'wait', server_name], timeout=15).strip()
                if exit_code != '0':
                    result['inflight_shutdown_exit'] = exit_code
                    result['inflight_shutdown_state'] = json.loads(command(['inspect', server_name]))[0]['State']
                    raise RuntimeError('In-flight Login SIGTERM failed: ' + exit_code)
                with psycopg.connect(dbname=dbname, **admin) as conn:
                    audit_rows = conn.execute('SELECT count(*) FROM app_global."TLOG" WHERE "dwUserID"=205').fetchone()[0]
                    current_rows = conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=205').fetchone()[0]
                    if audit_rows != 1 or current_rows != 0: raise RuntimeError('Shutdown did not drain committed authentication and cleanup')
                result['inflight_shutdown'] = {'audit_rows': audit_rows, 'current_sessions': current_rows,
                                                'health_responsive_during_database_wait': True}
            logs = execute(['podman', 'logs', server_name])
            reject_sanitizer_errors(logs)
            result['login_server'] = {'health_http_status': 200, 'sigterm_exit_code': 0,
                                     'image': args.image, 'binary': server_binary, 'log': scrub(logs)}
            result['status'] = 'passed'
            print('tloginsvr_asio: native login, encrypted protocol and SIGTERM passed', flush=True)
            return

        config = ('[server]\nport=0\n[content]\nconnection_string_env="FOURSTORY_CONTENT_CONNECTION"\n'
                  f'manifest_sha256="{manifest}"\n[health]\nport=18916\n[metrics]\nport=0\n'
                  '[shutdown]\ndrain_ms=50\n[log]\nlevel="info"\n')
        (public / 'wrong-manifest.toml').write_text(config.replace(manifest, '0' * 64))
        (public / 'wrong-manifest.toml').chmod(0o644)
        rejected = subprocess.run(base_run + [server_binary, '--config',
                                  '/run/fourstory-test/wrong-manifest.toml'], text=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        if (rejected.returncode == 0 or 'does not match the configured verified manifest' not in rejected.stdout
                or 'listener on' in rejected.stdout):
            raise RuntimeError('Map did not refuse wrong catalog before listening: ' + scrub(rejected.stdout))
        result['map_rejected_manifest'] = {'exit_code': rejected.returncode, 'log': scrub(rejected.stdout)}
        (public / 'map.toml').write_text(config)
        (public / 'map.toml').chmod(0o644)
        command(['run', '-d', '--name', server_name, '--label', LABEL + '=' + state['token'],
                 '--network', state['network'], '-p', '127.0.0.1::18916',
                 '-v', str(repo) + ':/src:ro,z', '-v', str(public) + ':/run/fourstory-test:ro,z',
                 '--env-file', str(private / 'native.env'), args.image,
                 server_binary, '--config', '/run/fourstory-test/map.toml'])
        port = command(['port', server_name, '18916/tcp']).rsplit(':', 1)[1]
        deadline = time.monotonic() + 45
        while True:
            try:
                with urllib.request.urlopen('http://127.0.0.1:' + port + '/healthz', timeout=1) as response:
                    if response.status == 200:break
            except (OSError, TimeoutError):pass
            running = json.loads(command(['inspect', server_name]))[0]['State']['Running']
            if not running or time.monotonic() > deadline:
                raise RuntimeError('Map startup/health failed: ' + scrub(execute(['podman', 'logs', server_name])))
            time.sleep(0.2)
        command(['kill', '--signal', 'TERM', server_name])
        exit_code = execute(['podman', 'wait', server_name], timeout=15).strip()
        logs = execute(['podman', 'logs', server_name])
        if exit_code != '0' or 'PostgreSQL catalog ready:' not in logs or 'spawn_manager: spawned' not in logs:
            raise RuntimeError('Map runtime/shutdown verification failed: ' + scrub(logs))
        result['map_server'] = {'health_http_status': 200, 'sigterm_exit_code': 0,
                                'image': args.image, 'binary': server_binary, 'log': scrub(logs)}
        print('tmapsvr_asio: PostgreSQL catalog startup, health and SIGTERM passed', flush=True)
        result['status'] = 'passed'
    except Exception as error:
        result['status'] = 'failed'
        # Do not expose arbitrary backend diagnostics from control-plane SQL.
        result['error'] = scrub(str(error)) if isinstance(error, RuntimeError) else type(error).__name__
        import traceback
        result['error_location'] = [{'file': Path(f.filename).name, 'line': f.lineno} for f in traceback.extract_tb(error.__traceback__)]
        raise
    finally:
        if subprocess.run(['podman', 'container', 'exists', server_name], capture_output=True).returncode == 0:
            verify_container(state, server_name)
            if result.get('status') != 'passed':
                result['server_failure_log'] = scrub(execute(['podman', 'logs', server_name]))
            command(['rm', '--force', server_name])
        if subprocess.run(['podman', 'container', 'exists', smtp_name], capture_output=True).returncode == 0:
            verify_container(state, smtp_name)
            command(['rm', '--force', smtp_name])
        if spool.exists():
            shutil.rmtree(spool)
        for env_file in ('native.env', 'server.env'):
            (private / env_file).unlink(missing_ok=True)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n')
        print('Verification report: ' + str(args.report), flush=True)


if __name__ == '__main__':
    try:
        main()
    except Exception:
        print('Native verification failed; inspect the scrubbed report.', file=sys.stderr)
        sys.exit(1)
