#!/usr/bin/env python3
"""Real Log sink/UDP verification with synthetic PostgreSQL and lost COMMIT replies.

Use an owned disposable_environment.py --postgresql-only lab. psycopg 3 required.
This fixture is not a production audit migration. TLS is disabled only on loopback
so the proxy can discard CommandComplete after PostgreSQL executes COMMIT.
"""
import argparse, contextlib, hashlib, json, re, socket, struct, subprocess, threading, time, uuid
import urllib.request
from pathlib import Path
from disposable_environment import command, private_file, verify_container, LABEL


def receive(sock, size):
    data = b''
    while len(data) < size:
        more = sock.recv(size-len(data))
        if not more: raise EOFError()
        data += more
    return data


class LostReplyProxy:
    def __init__(self, pg_port, operation):
        self.pg_port, self.operation = pg_port, operation
        self.listener = socket.socket(); self.listener.bind(('127.0.0.1', 0)); self.listener.listen()
        self.port = self.listener.getsockname()[1]
        self.stop = threading.Event(); self.clients = []; self.threads = []; self.drops = 0
        self.acceptor = threading.Thread(target=self.accept, daemon=True); self.acceptor.start()

    def accept(self):
        self.listener.settimeout(.2)
        while not self.stop.is_set():
            try: client, _ = self.listener.accept()
            except socket.timeout: continue
            except OSError: break
            upstream = socket.create_connection(('127.0.0.1', self.pg_port)); self.clients += [client, upstream]
            try:
                size = receive(client, 4)
                upstream.sendall(size + receive(client, struct.unpack('!I', size)[0]-4))
            except (OSError, EOFError):
                client.close(); upstream.close(); continue
            for src, dst, response in [(client, upstream, False), (upstream, client, True)]:
                thread = threading.Thread(target=self.relay, args=(src, dst, response), daemon=True)
                self.threads.append(thread); thread.start()

    def relay(self, src, dst, response):
        try:
            while not self.stop.is_set():
                kind, length = receive(src, 1), receive(src, 4)
                body = receive(src, struct.unpack('!I', length)[0]-4)
                if response and kind == b'C' and body.startswith(self.operation):
                    self.drops += 1
                    return
                dst.sendall(kind + length + body)
        except (OSError, EOFError): pass
        finally:
            for conn in (src, dst):
                with contextlib.suppress(OSError): conn.shutdown(socket.SHUT_RDWR)
                conn.close()

    def close(self):
        self.stop.set(); self.listener.close()
        for conn in self.clients:
            with contextlib.suppress(OSError): conn.shutdown(socket.SHUT_RDWR)
            conn.close()
        self.acceptor.join(2)
        for thread in self.threads: thread.join(2)


def schema(conn):
    # Test-owned schema, not a production migration or backup reconstruction.
    columns = ['lt_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY', 'lt_logdate timestamp NOT NULL',
        'lt_serverid bigint NOT NULL', 'lt_clientip text NOT NULL', 'lt_action bigint NOT NULL',
        'lt_mapid integer NOT NULL', 'lt_x integer', 'lt_y integer', 'lt_z integer']
    columns += [f'lt_dwkey{i} bigint' for i in range(1, 12)]
    columns += [f'lt_key{i} text' for i in range(1, 8)]
    columns += ['lt_fmt bigint', 'lt_log bytea CHECK(octet_length(lt_log)<=512)']
    conn.execute('CREATE TABLE "TLOG_AUDIT" (' + ','.join(columns) + ')')


def datagram(action, payload):
    # Independent offsets from LogPacket.h and original Win32 MSVC alignment.
    log = bytearray(500)
    struct.pack_into('<6HI', log, 0, 2026, 10, 9, 12, 34, 56, 0)
    struct.pack_into('<I', log, 16, 0xffffffff); log[20:29] = b'127.0.0.1'
    struct.pack_into('<IH', log, 36, action, 65535)
    struct.pack_into('<iii', log, 44, -10, 20, -30)
    struct.pack_into('<11q', log, 56, *range(11)); log[144:156] = b"source'\\test"
    struct.pack_into('<I', log, 496, 4)
    return struct.pack('<HHIIH20sH', 536+len(payload), 0, 0, 42, 0, b'127.0.0.1', 0) + log + payload


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work', type=Path, required=True); p.add_argument('--build-dir', default='build/linux-debug')
    p.add_argument('--image', default='localhost/fourstory:build-deps')
    p.add_argument('--runtime-bin-dir', help='Installed Log directory; backend test still uses --build-dir')
    p.add_argument('--report', type=Path, required=True); args = p.parse_args()
    import psycopg
    from psycopg.conninfo import make_conninfo
    from psycopg import sql
    repo = Path(__file__).resolve().parents[2]; work = args.work.resolve()
    if work.is_relative_to(repo): raise RuntimeError('Private lab must be outside repository')
    state = json.loads((work/'state.json').read_text()); verify_container(state, state['pg_container'])
    import fcntl
    lock = (work/'.native-verification.lock').open('a'); fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    values = dict(line.split('=', 1) for line in (work/'postgres.env').read_text().splitlines())
    run = uuid.uuid4().hex[:12]; private = work/('log-'+run); private.mkdir(mode=0o700)
    base = dict(host='127.0.0.1', port=state['pg_port'], dbname='postgres', user='postgres',
        password=values['POSTGRES_PASSWORD'], sslmode='disable', connect_timeout=3)
    dbname = 'audit_'+run; image = json.loads(command(['image','inspect',args.image]))[0]['Id']
    report = {'scope':'Log transaction outcome and synthetic native PostgreSQL/UDP verification',
        'production_schema_verified':False, 'original_client_verified':False, 'completed':False,
        'tls':'disabled on loopback for deterministic PostgreSQL wire fault injection',
        'image':{'reference':args.image,'id':image}, 'build_dir':args.build_dir, 'checks':[]}
    checks = report['checks']
    def check(ok, label):
        checks.append({'name':label,'passed':bool(ok)})
        if not ok: raise RuntimeError(label)
    proxies = []; daemon = None
    try:
        with psycopg.connect(**base, autocommit=True) as admin:
            admin.execute(sql.SQL('CREATE DATABASE {}').format(sql.Identifier(dbname)))
        base['dbname'] = dbname
        with psycopg.connect(**base, autocommit=True) as conn:
            schema(conn)
            for operation in (b'COMMIT', b'INSERT'): proxies.append(LostReplyProxy(int(state['pg_port']), operation))
            env = private/'test.env'
            private_file(env, '\n'.join([
                'TLOGSVR_TEST_POSTGRESQL_CONN='+make_conninfo(**base),
                'TLOGSVR_TEST_LOST_COMMIT_CONN='+make_conninfo(**(base|{'port':proxies[0].port})),
                'TLOGSVR_TEST_LOST_INSERT_CONN='+make_conninfo(**(base|{'port':proxies[1].port})),
                'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1'])+'\n')
            completed = subprocess.run(['podman','run','--rm','--network','host','--userns','keep-id',
                '--label',LABEL+'='+state['token'],'--env-file',str(env),'-v',str(repo)+':/src:z',image,
                '/src/'+args.build_dir+'/bin/test_tlogsvr_postgresql_sink'], text=True, capture_output=True)
            output = (completed.stdout+completed.stderr).replace(base['password'],'[REDACTED]')
            private_file(private/'backend.log', output)
            results = re.search(r'Results: (\d+) passed, (\d+) failed', output)
            report['backend'] = {'passed':int(results[1]) if results else 0, 'failed':int(results[2]) if results else None,
                'log_sha256':hashlib.sha256(output.encode()).hexdigest()}
            check(completed.returncode==0 and results and results[2]=='0', 'native C++ PostgreSQL sink checks')
            check(proxies[0].drops==2, 'single and bulk COMMIT replies lost after PostgreSQL completed them')
            check(proxies[1].drops==1, 'INSERT reply lost before COMMIT')
            for proxy in proxies: proxy.close()
            proxies=[]
            udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); udp.bind(('127.0.0.1',0)); port=udp.getsockname()[1]; udp.close()
            config='target_table="TLOG_AUDIT"\n[server]\nbind="127.0.0.1"\nport='+str(port)+'\n'
            config+='[database]\nbackend="postgresql"\nconnection_string='+json.dumps(make_conninfo(**base))+'\npool_size=2\nworker_threads=1\n'
            health=socket.socket(); health.bind(('127.0.0.1',0)); health_port=health.getsockname()[1]; health.close()
            config+='[health]\nport='+str(health_port)+'\n'
            config+='[retry]\nmax_queue=32\ndrain_interval_secs=1\ndrain_batch_size=8\n[backpressure]\nsample_interval_secs=0\n'
            private_file(private/'log.toml',config)
            binary=(args.runtime_bin_dir or '/src/'+args.build_dir+'/bin')+'/tlogsvr_asio'
            daemon=command(['run','-d','--name','fourstory-log-outcomes-'+run,'--network','host','--userns',
                ('keep-id:uid=10001,gid=10001' if args.runtime_bin_dir else 'keep-id'),
                '--label',LABEL+'='+state['token'],'-v',str(repo)+':/src:z','-v',str(private)+':/lab:z',
                '-e','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','-e','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1',
                image,binary,'--config','/lab/log.toml'])
            deadline=time.monotonic()+20
            while 'log server listening UDP' not in command(['logs',daemon]):
                if time.monotonic()>deadline: raise RuntimeError('UDP daemon startup timed out')
                time.sleep(.1)
            sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            for n,payload in enumerate((bytes(range(256))*2,b'')): sock.sendto(datagram(900+n,payload),('127.0.0.1',port))
            sock.sendto(b'bad',('127.0.0.1',port)); sock.close()
            deadline=time.monotonic()+10
            while conn.execute('SELECT count(*) FROM "TLOG_AUDIT"').fetchone()[0]!=2:
                if time.monotonic()>deadline: raise RuntimeError('UDP persistence timed out')
                time.sleep(.05)
            rows=conn.execute('SELECT lt_serverid,lt_action,lt_mapid,lt_log,lt_key1 FROM "TLOG_AUDIT" ORDER BY lt_action').fetchall()
            check(rows[0]==(4294967295,900,65535,bytes(range(256))*2,"source'\\test"), 'original-layout UDP persists exact native fields')
            check(rows[1][3] is None,'empty UDP payload persists as NULL')
            # Force a safe failure, then hold its periodic retry inside PostgreSQL.
            # Health must still respond while that actual bulk INSERT is sleeping.
            conn.execute('ALTER TABLE "TLOG_AUDIT" ADD CONSTRAINT retry_test CHECK(lt_action<>902)')
            sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            sock.sendto(datagram(902,b'retry'),('127.0.0.1',port)); sock.close()
            deadline=time.monotonic()+5
            while 'buffering for retry' not in command(['logs',daemon]):
                if time.monotonic()>deadline: raise RuntimeError('Safe retry was not buffered')
                time.sleep(.05)
            conn.execute("CREATE FUNCTION hold_retry() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN "
                         "IF NEW.lt_action=902 THEN PERFORM pg_sleep(2); END IF; RETURN NEW; END $$")
            conn.execute('CREATE TRIGGER hold_retry BEFORE INSERT ON "TLOG_AUDIT" FOR EACH ROW EXECUTE FUNCTION hold_retry()')
            conn.execute('ALTER TABLE "TLOG_AUDIT" DROP CONSTRAINT retry_test')
            deadline=time.monotonic()+5
            while not conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() "
                                   "AND wait_event='PgSleep'").fetchone()[0]:
                if time.monotonic()>deadline: raise RuntimeError('Periodic retry did not reach blocking DB operation')
                time.sleep(.025)
            with urllib.request.urlopen('http://127.0.0.1:'+str(health_port)+'/healthz', timeout=.75) as response:
                check(response.status==200,'health reactor responds during a blocked periodic database drain')
            deadline=time.monotonic()+5
            while conn.execute('SELECT count(*) FROM "TLOG_AUDIT" WHERE lt_action=902').fetchone()[0]!=1:
                if time.monotonic()>deadline: raise RuntimeError('Periodic retry did not commit')
                time.sleep(.05)
            check(conn.execute('SELECT lt_log FROM "TLOG_AUDIT" WHERE lt_action=902').fetchone()[0]==b'retry',
                  'periodic worker retry recovers exact UDP payload after confirmed rollback')
            command(['stop','--time','10',daemon])
            logs=command(['logs',daemon]).replace(base['password'],'[REDACTED]'); private_file(private/'daemon.log',logs)
            check('received=4 drops_bad_format=1 inserted=2 enqueued=1 drained=1' in logs,'daemon separates malformed datagram and confirmed inserts')
            status=json.loads(command(['inspect',daemon]))[0]['State']
            check(status['ExitCode']==0 and 'outcome_unknown=0' in logs,'SIGTERM joins accepted work and reports outcome counters')
            check(not any(x in logs+output for x in ['ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:']), 'no sanitizer diagnostics')
            report['daemon_log_sha256']=hashlib.sha256(logs.encode()).hexdigest()
            command(['rm',daemon]); daemon=None
            report['completed']=True
    finally:
        for proxy in proxies: proxy.close()
        if daemon:
            logs=command(['logs',daemon]).replace(base['password'],'[REDACTED]')
            private_file(private/'failure-daemon.log',logs)
            report['failure_daemon_log_sha256']=hashlib.sha256(logs.encode()).hexdigest()
            command(['rm','-f',daemon])
        base['dbname']='postgres'
        with psycopg.connect(**base,autocommit=True) as admin:
            admin.execute(sql.SQL('DROP DATABASE IF EXISTS {} WITH (FORCE)').format(sql.Identifier(dbname)))
        for path in (private/'test.env',private/'log.toml'): path.unlink(missing_ok=True)
        report['owned_database_removed']=True
        args.report.parent.mkdir(parents=True,exist_ok=True); args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'backend':report['backend'],'checks':len(checks),'passed':all(c['passed'] for c in checks)}))


if __name__=='__main__': main()
