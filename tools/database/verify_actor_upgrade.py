#!/usr/bin/env python3
"""Verify 030 -> 031 and certified actor publication in an owned disposable PG lab.

Requires private old (two-table) and new (four-table) backup manifests. Never
updates an existing player world or modifies imported rows. Creates/removes only
one uniquely named verification database, and publishes aggregate evidence.
"""
import argparse
import json
from pathlib import Path
import shutil
import tempfile
import uuid
from activate_actor_catalog import activate_actor_catalog
from activate_catalog import MAPPING
from disposable_environment import verify_container
from migrate import apply_migrations, import_snapshot, load_snapshot, read_target, table_hash


def main():
    import psycopg
    from psycopg import sql
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work',type=Path,required=True)
    p.add_argument('--previous-manifest',type=Path,required=True)
    p.add_argument('--manifest',type=Path,required=True)
    p.add_argument('--report',type=Path,required=True)
    a=p.parse_args();repo=Path(__file__).resolve().parents[2]
    state=json.loads((a.work/'state.json').read_text());verify_container(state,state['pg_container'])
    values=dict(l.split('=',1) for l in (a.work/'postgres.env').read_text().splitlines())
    admin=dict(host='127.0.0.1',port=state['pg_port'],user='postgres',password=values['POSTGRES_PASSWORD'],autocommit=True)
    db='fourstory_actor_upgrade_'+uuid.uuid4().hex[:12];checks=[];report={'status':'failed','checks':checks}
    def check(ok,label):
        if not ok:raise RuntimeError(label)
        checks.append(label)
    with psycopg.connect(dbname='postgres',**admin) as c:c.execute(sql.SQL('CREATE DATABASE {}').format(sql.Identifier(db)))
    try:
        with psycopg.connect(dbname=db,**admin) as c,tempfile.TemporaryDirectory(prefix='fourstory-pre031-') as tmp:
            before=Path(tmp)
            for source in (repo/'database/postgresql').glob('[0-9][0-9][0-9]-*.sql'):
                if int(source.name[:3])<=30:shutil.copyfile(source,before/source.name)
            check(len(apply_migrations(c,before))==30,'original 001-030 apply unchanged')
            old=import_snapshot(c,a.previous_manifest,MAPPING)
            _,tables=load_snapshot(a.previous_manifest,MAPPING)
            check({t['table'] for t,_,_ in tables}=={'TITEMMAGICCHART','TSKILLPOINTCHART'},'old manifest is exactly the previous actor contract')
            # Reconstruct only the old selector and its import receipt; no
            # runtime player/owner row or transfer graph is rewritten.
            c.execute('INSERT INTO runtime_control.actor_catalog(singleton,run_id) VALUES(true,%s)',(old['run_id'],))
            receipts=c.execute('SELECT * FROM migration_control.applied ORDER BY name').fetchall()
            old_hashes=[table_hash(read_target(c,model,old['run_id']),model['column_mapping']) for _,model,_ in tables]
            # Keep this historical 030 -> 031 proof bounded when subsequent
            # migrations are added; all files retain their original contents.
            source=repo/'database/postgresql/031-actor-graph-compatibility.sql'
            shutil.copyfile(source,before/source.name)
            check(apply_migrations(c,before)==['031-actor-graph-compatibility.sql'],'upgrade applies only new migration 031')
            check(c.execute('SELECT * FROM migration_control.applied WHERE name<%s ORDER BY name',('031',)).fetchall()==receipts,'all 30 prior receipt hashes and timestamps are unchanged')
            check(apply_migrations(c,before)==[],'second migration run is a no-op')
            check(c.execute('SELECT count(*) FROM actor_compat.statistics_release').fetchone()==(0,),'old release does not falsely satisfy new stat contract')
            new=import_snapshot(c,a.manifest,MAPPING)
            published=activate_actor_catalog(c,a.manifest,previous_manifest=a.previous_manifest)
            check(published['reference_tables']==4 and published['run_id']==new['run_id'],'complete verified four-table release activates')
            check(c.execute('SELECT run_id FROM actor_compat.statistics_release').fetchone()==(new['run_id'],),'native runtime readiness identifies the new complete release')
            check(published['compatibility_created'],'explicit old manifest creates a directional additive certificate')
            check(c.execute('SELECT source_manifest_sha256,target_manifest_sha256 FROM actor_compat.transfer_catalog_compatibility').fetchone()==
                  (published['compatible_previous_manifest'],published['manifest_sha256']),'runtime projection exposes only the verified exact old/current pair')
            check(not activate_actor_catalog(c,a.manifest,previous_manifest=a.previous_manifest)['changed'],'repeated activation adds no duplicate release')
            check([table_hash(read_target(c,model,old['run_id']),model['column_mapping']) for _,model,_ in tables]==old_hashes,'prior imported values remain unchanged after upgrade and activation')
            check([table_hash(read_target(c,model,new['run_id']),model['column_mapping']) for _,model,_ in tables]==old_hashes,'previous item-magic and skill-point catalogs are identical in the extended release')
            report.update(status='passed',activation=published,scope='Migration and actual pinned-backup proof verification. Player recovery is exercised separately by actor-transition-{debug,asan,release}.json; no original client executable.')
    finally:
        with psycopg.connect(dbname='postgres',**admin) as c:c.execute(sql.SQL('DROP DATABASE {} WITH (FORCE)').format(sql.Identifier(db)))
        report['owned_database_removed']=True
        a.report.write_text(json.dumps(report,indent=2)+'\n')
    print(f'Actor catalog upgrade: {len(checks)} checks passed')

if __name__=='__main__':main()
