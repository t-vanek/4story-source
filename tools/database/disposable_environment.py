#!/usr/bin/env python3
"""Create/stop an owned Podman forensic environment, or run a PG tool within it.

All database state is disposable. Backups are copied, never mounted writable.
Credentials live only in a mode-0700 work directory outside the repository.
Stop removes only containers/network carrying this environment's unique label.
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time
import uuid

LABEL='fourstory.database-forensics'


def command(args):
    return subprocess.check_output(['podman',*args],text=True,stderr=subprocess.PIPE).strip()


def private_file(path,text):
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w') as f:f.write(text)


def start(work,repo,postgresql_only=False):
    if work.resolve().is_relative_to(repo.resolve()):raise RuntimeError('WORK_DIRECTORY_MUST_BE_OUTSIDE_REPOSITORY')
    work.mkdir(mode=0o700,parents=True,exist_ok=False)
    token=uuid.uuid4().hex;prefix='fourstory-db-'+token[:12]
    state={'token':token,'network':prefix,'pg_container':prefix+'-postgres'}
    if not postgresql_only:state['mssql_container']=prefix+'-mssql'
    private_file(work/'state.json',json.dumps(state))
    if not postgresql_only:
        private_file(work/'mssql.env','ACCEPT_EULA=Y\nMSSQL_PID=Developer\nMSSQL_MEMORY_LIMIT_MB=3072\nMSSQL_SA_PASSWORD='+secrets.token_urlsafe(30)+'aA1!\n')
    private_file(work/'postgres.env','POSTGRES_DB=fourstory_reconstruction\nPOSTGRES_PASSWORD='+secrets.token_urlsafe(32)+'\n')
    command(['network','create','--label',LABEL+'='+token,state['network']])
    if not postgresql_only:
        command(['run','-d','--name',state['mssql_container'],'--label',LABEL+'='+token,'--network',state['network'],
                 '--env-file',str(work/'mssql.env'),'mcr.microsoft.com/mssql/server:2022-latest'])
        for db in ('TGLOBAL_RAGEZONE','TGAME_RAGEZONE'):
            source=repo/(db+'.bak')
            if not source.is_file():raise RuntimeError('SOURCE_BACKUP_MISSING')
            command(['cp',str(source),state['mssql_container']+':/var/opt/mssql/'+source.name])
    command(['run','-d','--name',state['pg_container'],'--label',LABEL+'='+token,'--network',state['network'],
             '--env-file',str(work/'postgres.env'),'-p','127.0.0.1::5432','docker.io/library/postgres:18'])
    state['pg_port']=command(['port',state['pg_container'],'5432/tcp']).rsplit(':',1)[1]
    (work/'state.json').write_text(json.dumps(state))
    deadline=time.monotonic()+45
    while True:
        pg=subprocess.run(['podman','exec',state['pg_container'],'pg_isready','-U','postgres'],capture_output=True)
        sql_ready=True
        if not postgresql_only:
            sql=subprocess.run(['podman','exec',state['mssql_container'],'sh','-c',
                'SQLCMDPASSWORD="$MSSQL_SA_PASSWORD" exec /opt/mssql-tools18/bin/sqlcmd -S localhost -U sa -C -b -l 2 -Q "SELECT 1"'],capture_output=True)
            sql_ready=sql.returncode==0
        if pg.returncode==0 and sql_ready:break
        if time.monotonic()>deadline:raise RuntimeError('DATABASE_READINESS_TIMEOUT')
        time.sleep(0.5)
    print(json.dumps({'work_directory':str(work),'mssql_container':state.get('mssql_container'),'pg_container':state['pg_container']}))
    print('PostgreSQL is ready.' if postgresql_only else 'SQL Server and PostgreSQL are ready.')


def verify_container(state,name):
    data=json.loads(command(['inspect',name]))[0]
    if data['Config']['Labels'].get(LABEL)!=state['token']:raise RuntimeError('CONTAINER_OWNERSHIP_MISMATCH')


def stop(work,state):
    for key in ('mssql_container','pg_container'):
        if key not in state:continue
        name=state[key]
        if subprocess.run(['podman','container','exists',name],capture_output=True).returncode==0:
            verify_container(state,name);command(['rm','--force','--volumes',name])
    if subprocess.run(['podman','network','exists',state['network']],capture_output=True).returncode==0:
        network=json.loads(command(['network','inspect',state['network']]))[0]
        if network.get('labels',{}).get(LABEL)!=state['token']:raise RuntimeError('NETWORK_OWNERSHIP_MISMATCH')
        command(['network','rm',state['network']])
    print('Owned database containers and network removed; private evidence directory retained.')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['start','run-pg','stop'])
    p.add_argument('--work',type=Path,required=True)
    p.add_argument('--repo',type=Path,default=Path('.'))
    p.add_argument('--postgresql-only',action='store_true',help='Create the native runtime lab without any SQL Server container')
    argv=sys.argv[1:]
    separator=argv.index('--') if '--' in argv else len(argv)
    args=p.parse_args(argv[:separator]);cmd=argv[separator+1:];work=args.work.resolve()
    try:
        if args.action=='start':start(work,args.repo.resolve(),args.postgresql_only);return
        state=json.loads((work/'state.json').read_text())
        if args.action=='stop':stop(work,state);return
        verify_container(state,state['pg_container'])
        values=dict(line.split('=',1) for line in (work/'postgres.env').read_text().splitlines())
        env=os.environ.copy();env.update(PGHOST='127.0.0.1',PGPORT=state['pg_port'],PGDATABASE='fourstory_reconstruction',
                                         PGUSER='postgres',PGPASSWORD=values['POSTGRES_PASSWORD'])
        if not cmd:raise RuntimeError('COMMAND_REQUIRED')
        raise SystemExit(subprocess.run(cmd,env=env).returncode)
    except Exception:
        # Runtime/CLI errors may include DSN or environment; keep them private.
        print('Disposable environment operation failed; inspect the private state and Podman status.')
        raise SystemExit(1)


if __name__=='__main__':main()
