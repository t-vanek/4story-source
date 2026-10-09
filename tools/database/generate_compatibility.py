#!/usr/bin/env python3
"""Generate reviewed PG catalog views without rewriting applied migrations.

Only the explicit reference allowlist is published. Original snapshots retain
their MSSQL column values and names. Each view is a join (not writable), and
separate lower-case aliases support the rewrite's unquoted SQL statements.
"""
import argparse
import json
from pathlib import Path

from generate_migrations import qi, REFERENCE_TABLES


def generate(mapping):
    models = {t['table']: t for t in mapping['tables'] if t['reference_import_allowed'] and t['database']=='TGAME_RAGEZONE' and t['schema']=='dbo' and t['table'] in REFERENCE_TABLES}
    if set(models) != set(REFERENCE_TABLES):
        raise RuntimeError('REFERENCE_CONTRACT_SET_CHANGED')
    statements = [
        '-- Migration 006: derived read-only current C++ catalog contracts.',
        '-- Metadata SHA256 ' + mapping['metadata_sha256'] + '.',
        '-- Do not edit applied SQL; add a new migration for a changed contract.',
        '-- No original snapshot row or type is modified.',
    ]
    for name in sorted(models):
        model = models[name]
        columns = [c['target'] for c in model['column_mapping']]
        if len({c.lower() for c in columns}) != len(columns):
            raise RuntimeError('CASE_FOLDED_COLUMN_COLLISION')
        if name == 'TMONATTRCHART':
            projection = [f'm.{qi(c)} AS {qi(c)}' if c == 'wID' else f'a.{qi(c)}' for c in columns]
            source = '''legacy_game."TMONATTRCHART" a
JOIN legacy_game."TMONSTERCHART" m
  ON m."_import_run_id" = a."_import_run_id"
 AND m."wMonAttr" = a."wID" AND m."bLevel" = a."bLevel"'''
            explanation = ('PROPOSED compatibility transform: wID is monster ID; stats come from the backup '
                           'attribute family wMonAttr at the template bLevel. Legacy TMap.cpp FindMonAttr '
                           'and current spawn_manager.cpp attrs.Find. No missing stats are fabricated.')
        else:
            projection = [f'a.{qi(c)}' for c in columns]
            source = qi(model['target_schema']) + '.' + qi(name) + ' a'
            explanation = 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.'
        statements.append('CREATE VIEW game_compat.' + qi(name) + ' AS\nSELECT ' + ', '.join(projection) +
                          '\nFROM ' + source + '\nJOIN runtime_control.active_catalog r ON '
                          'r.singleton AND r.run_id = a."_import_run_id";')
        statements.append('COMMENT ON VIEW game_compat.' + qi(name) + " IS '" + explanation.replace("'", "''") + "';")
        statements.append('CREATE VIEW game_compat.' + qi(name.lower()) + ' AS\nSELECT ' +
                          ', '.join(qi(c) + ' AS ' + qi(c.lower()) for c in columns) +
                          '\nFROM game_compat.' + qi(name) + ';')
    statements.append('''CREATE VIEW runtime_control.missing_monster_attributes AS
SELECT m."wID" AS monster_id, m."wMonAttr" AS attribute_id, m."bLevel" AS level
FROM legacy_game."TMONSTERCHART" m
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = m."_import_run_id"
LEFT JOIN legacy_game."TMONATTRCHART" a
  ON a."_import_run_id" = m."_import_run_id"
 AND a."wID" = m."wMonAttr" AND a."bLevel" = m."bLevel"
WHERE a."_source_row_number" IS NULL;
COMMENT ON VIEW runtime_control.missing_monster_attributes IS
  'Source catalog gaps retained for diagnosis; no placeholder combat values are introduced';
REVOKE ALL ON ALL TABLES IN SCHEMA game_compat, runtime_control FROM PUBLIC;''')
    return '\n\n'.join(statements) + '\n'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mapping', type=Path, default=Path('database/postgresql/mapping.json'))
    p.add_argument('--output', type=Path, default=Path('database/postgresql/006-current-chart-contracts.sql'))
    args = p.parse_args()
    args.output.write_text(generate(json.loads(args.mapping.read_text())))
    print('Generated 15 read-only catalog contracts and 15 case-folded aliases.')


if __name__ == '__main__':
    main()
