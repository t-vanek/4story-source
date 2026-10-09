#!/usr/bin/env python3
"""Inspect/restore ONLY into a newly created, disposable SQL Server container.

Uses the SA password already inside that container, never prints credentials or
rows. Full module definitions are kept in a mode-0700 private work directory;
only schema metadata, hashes and aggregate counts may be published.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess


def query(container, sql, database="master", tabular=False):
    args = ["podman", "exec", "-i", container, "sh", "-c",
            'SQLCMDPASSWORD="$MSSQL_SA_PASSWORD" exec /opt/mssql-tools18/bin/sqlcmd '
            '-S localhost -U sa -C -b -r1 -w 65535 ' +
            ('-s "|" -W' if tabular else '-y 0') + ' -d "$1"',
            "sqlcmd", database]
    result = subprocess.run(args, input="SET NOCOUNT ON;\n" + sql + "\nGO\n",
                            text=True, capture_output=True, check=False)
    if result.returncode:
        # SQL text and data deliberately absent from the raised exception.
        raise RuntimeError(f"sqlcmd failed (exit {result.returncode}): {result.stderr[:800]}")
    if tabular:
        lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        keys = lines[0].split("|")
        return [dict(zip(keys, (None if v == "NULL" else v for v in line.split("|")), strict=True))
                for line in lines[2:] if not line.startswith("Changed database")]
    lines = result.stdout.splitlines()
    start = next((i for i,line in enumerate(lines) if line.lstrip().startswith('[')),None)
    if start is None:
        # sqlcmd emits no JSON row for an empty FOR JSON result.
        if all(not line.strip() or line.startswith(('JSON_','---')) for line in lines):
            return []
        raise RuntimeError('Expected JSON result, received non-JSON output')
    return json.loads("".join(lines[start:]).strip())


CATALOG = {
    "settings": """SELECT DB_NAME() name, compatibility_level, collation_name,
        recovery_model_desc, state_desc, is_read_only FROM sys.databases WHERE name=DB_NAME()""",
    "columns": """SELECT s.name schema_name,t.name table_name,c.column_id ordinal,c.name column_name,
        TYPE_NAME(c.user_type_id) type_name,c.max_length,c.precision,c.scale,
        CAST(c.is_nullable AS int) is_nullable,CAST(c.is_identity AS int) is_identity,
        CAST(c.is_computed AS int) is_computed,cc.definition computed_definition,
        c.collation_name,CAST(CASE WHEN ic.column_id IS NULL THEN 0 ELSE 1 END AS int) is_pk,
        pk.name pk_name,df.definition default_definition,
        CONVERT(varchar(80),idc.seed_value) identity_seed,
        CONVERT(varchar(80),idc.increment_value) identity_increment,
        CONVERT(varchar(80),idc.last_value) identity_last_value
        FROM sys.tables t JOIN sys.schemas s ON s.schema_id=t.schema_id
        JOIN sys.columns c ON c.object_id=t.object_id
        LEFT JOIN sys.computed_columns cc ON cc.object_id=t.object_id AND cc.column_id=c.column_id
        LEFT JOIN sys.key_constraints pk ON pk.parent_object_id=t.object_id AND pk.type='PK'
        LEFT JOIN sys.index_columns ic ON ic.object_id=t.object_id AND ic.column_id=c.column_id
          AND ic.index_id=pk.unique_index_id
        LEFT JOIN sys.default_constraints df ON df.parent_object_id=t.object_id AND df.parent_column_id=c.column_id
        LEFT JOIN sys.identity_columns idc ON idc.object_id=t.object_id AND idc.column_id=c.column_id
        WHERE t.is_ms_shipped=0 ORDER BY s.name,t.name,c.column_id""",
    "indexes": """SELECT s.name schema_name,t.name table_name,i.name index_name,i.type_desc,
        CAST(i.is_unique AS int) is_unique,CAST(i.is_primary_key AS int) is_primary_key,
        CAST(i.is_unique_constraint AS int) is_unique_constraint,i.filter_definition,
        CAST(i.is_disabled AS int) is_disabled,
        (SELECT c.name column_name,ic.key_ordinal,CAST(ic.is_descending_key AS int) is_descending,
          CAST(ic.is_included_column AS int) is_included FROM sys.index_columns ic
          JOIN sys.columns c ON c.object_id=ic.object_id AND c.column_id=ic.column_id
          WHERE ic.object_id=i.object_id AND ic.index_id=i.index_id
          ORDER BY ic.key_ordinal,ic.index_column_id FOR JSON PATH) column_details
        FROM sys.indexes i JOIN sys.tables t ON t.object_id=i.object_id
        JOIN sys.schemas s ON s.schema_id=t.schema_id WHERE i.type>0 AND t.is_ms_shipped=0
        ORDER BY s.name,t.name,i.index_id""",
    "foreign_keys": """SELECT fk.name fk_name,OBJECT_SCHEMA_NAME(fk.parent_object_id) parent_schema,
        OBJECT_NAME(fk.parent_object_id) parent_table,pc.name parent_column,
        OBJECT_SCHEMA_NAME(fk.referenced_object_id) referenced_schema,
        OBJECT_NAME(fk.referenced_object_id) referenced_table,rc.name referenced_column,
        fkc.constraint_column_id ordinal,fk.delete_referential_action_desc delete_action,
        fk.update_referential_action_desc update_action,CAST(fk.is_disabled AS int) is_disabled,
        CAST(fk.is_not_trusted AS int) is_not_trusted FROM sys.foreign_keys fk
        JOIN sys.foreign_key_columns fkc ON fkc.constraint_object_id=fk.object_id
        JOIN sys.columns pc ON pc.object_id=fkc.parent_object_id AND pc.column_id=fkc.parent_column_id
        JOIN sys.columns rc ON rc.object_id=fkc.referenced_object_id AND rc.column_id=fkc.referenced_column_id
        ORDER BY fk.name,fkc.constraint_column_id""",
    "checks": """SELECT OBJECT_SCHEMA_NAME(parent_object_id) schema_name,
        OBJECT_NAME(parent_object_id) table_name,name,definition,
        CAST(is_disabled AS int) is_disabled,CAST(is_not_trusted AS int) is_not_trusted
        FROM sys.check_constraints ORDER BY name""",
    "modules": """SELECT s.name schema_name,o.name object_name,o.type_desc,o.type,
        o.create_date,o.modify_date,OBJECT_NAME(o.parent_object_id) parent_object,
        m.definition,CAST(m.uses_ansi_nulls AS int) uses_ansi_nulls,
        CAST(m.uses_quoted_identifier AS int) uses_quoted_identifier
        FROM sys.objects o JOIN sys.schemas s ON s.schema_id=o.schema_id
        LEFT JOIN sys.sql_modules m ON m.object_id=o.object_id
        WHERE o.is_ms_shipped=0 AND o.type IN ('P','V','FN','IF','TF','TR','PC','FS','FT')
        ORDER BY o.type,o.name""",
    "parameters": """SELECT OBJECT_SCHEMA_NAME(p.object_id) schema_name,
        OBJECT_NAME(p.object_id) object_name,p.parameter_id,p.name,
        TYPE_NAME(p.user_type_id) type_name,p.max_length,p.precision,p.scale,
        CAST(p.is_output AS int) is_output FROM sys.parameters p
        JOIN sys.objects o ON o.object_id=p.object_id WHERE o.is_ms_shipped=0
        ORDER BY object_name,p.parameter_id""",
    "dependencies": """SELECT OBJECT_SCHEMA_NAME(referencing_id) referencing_schema,
        OBJECT_NAME(referencing_id) referencing_object,referenced_server_name,
        referenced_database_name,referenced_schema_name,referenced_entity_name,
        CAST(is_ambiguous AS int) is_ambiguous FROM sys.sql_expression_dependencies
        ORDER BY referencing_object,referenced_entity_name""",
    "row_counts": """SELECT s.name schema_name,t.name table_name,SUM(p.rows) row_count
        FROM sys.tables t JOIN sys.schemas s ON s.schema_id=t.schema_id
        JOIN sys.partitions p ON p.object_id=t.object_id AND p.index_id IN (0,1)
        WHERE t.is_ms_shipped=0 GROUP BY s.name,t.name ORDER BY s.name,t.name""",
    "synonyms": "SELECT SCHEMA_NAME(schema_id) schema_name,name,base_object_name FROM sys.synonyms",
    "sequences": """SELECT SCHEMA_NAME(schema_id) schema_name,name,TYPE_NAME(user_type_id) type_name,
        CONVERT(varchar(80),start_value) start_value,CONVERT(varchar(80),increment) increment_value
        FROM sys.sequences""",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--container", required=True)
    parser.add_argument("--private-output", type=Path, required=True)
    parser.add_argument("--restore-disposable", action="store_true",
                        help="Require empty instance (only four system DBs); restore fixed forensic names")
    parser.add_argument("--resume-disposable", action="store_true",
                        help="Allow only the two fixed forensic databases plus system DBs; never replace them")
    args = parser.parse_args()
    args.private_output.mkdir(parents=True, mode=0o700, exist_ok=True)
    os.chmod(args.private_output, 0o700)
    live = query(args.container, "SELECT name FROM sys.databases FOR JSON PATH")
    existing={r['name'] for r in live}
    allowed={'master','model','msdb','tempdb'}
    if args.resume_disposable:
        allowed.update({'forensic_TGLOBAL_RAGEZONE','forensic_TGAME_RAGEZONE'})
    if args.restore_disposable and not existing.issubset(allowed):
        raise RuntimeError("Refusing restore: instance is not empty/disposable")
    report = {"engine": query(args.container, "SELECT CONVERT(varchar(100),SERVERPROPERTY('ProductVersion')) version, CONVERT(varchar(100),SERVERPROPERTY('Edition')) edition FOR JSON PATH"), "backups": {}}
    for name in ("TGLOBAL_RAGEZONE", "TGAME_RAGEZONE"):
        disk = f"/var/opt/mssql/{name}.bak"
        header = query(args.container, f"RESTORE HEADERONLY FROM DISK=N'{disk}'", tabular=True)
        files = query(args.container, f"RESTORE FILELISTONLY FROM DISK=N'{disk}' WITH FILE=1", tabular=True)
        # VERIFYONLY is a readability/completeness check, not DBCC integrity.
        subprocess_query = f"RESTORE VERIFYONLY FROM DISK=N'{disk}' WITH FILE=1; SELECT 'passed' verification FOR JSON PATH"
        verify = query(args.container, subprocess_query)
        info = {"headers":header, "files":files, "verifyonly":verify}
        if args.restore_disposable:
            target = "forensic_" + name
            moves = []
            for i, f in enumerate(files):
                logical = f['LogicalName'].replace("'", "''")
                ext = 'ldf' if f['Type'] == 'L' else 'mdf'
                moves.append(f"MOVE N'{logical}' TO N'/var/opt/mssql/data/{target}_{i}.{ext}'")
            statement = f"RESTORE DATABASE [{target}] FROM DISK=N'{disk}' WITH FILE=1," + ','.join(moves) + ",RECOVERY;"
            # sqlcmd emits restore progress before JSON; use plain call here.
            if target not in existing:
                result = subprocess.run(["podman","exec","-i",args.container,"sh","-c",
                    'SQLCMDPASSWORD="$MSSQL_SA_PASSWORD" exec /opt/mssql-tools18/bin/sqlcmd -S localhost -U sa -C -b -r1'],
                    input=statement+"\nGO\n", text=True, capture_output=True)
                (args.private_output / (name+".restore.log")).write_text(result.stdout+result.stderr)
                if result.returncode:
                    info['restore'] = 'failed'
                    report['backups'][name] = info
                    continue
            info['restore'] = 'passed'
            check = subprocess.run(["podman","exec","-i",args.container,"sh","-c",
                'SQLCMDPASSWORD="$MSSQL_SA_PASSWORD" exec /opt/mssql-tools18/bin/sqlcmd -S localhost -U sa -C -b -r1'],
                input=f"DBCC CHECKDB([{target}]) WITH NO_INFOMSGS, ALL_ERRORMSGS;\nGO\n", text=True, capture_output=True)
            (args.private_output / (name+".checkdb.log")).write_text(check.stdout+check.stderr)
            info['checkdb'] = {'exit_code':check.returncode,'output_empty':not (check.stdout+check.stderr).strip()}
            catalog = {key:query(args.container, sql+" FOR JSON PATH, INCLUDE_NULL_VALUES", target)
                       for key,sql in CATALOG.items()}
            (args.private_output / (name+".catalog.json")).write_text(json.dumps(catalog,ensure_ascii=False,indent=2))
        report['backups'][name] = info
        print(f"{name}: VERIFYONLY {info['verifyonly']}; restore {info.get('restore','not requested')}")
    (args.private_output / "backup-report.json").write_text(json.dumps(report,indent=2))


if __name__ == "__main__":
    main()
