"""Run against a disposable MySQL/MariaDB instance; creates unique test databases.

Example: python tests/test_sql_migration.py --mysql mysql --port 33379
Authentication uses the client's normal MYSQL_PWD environment variable if set.
"""
import argparse
from pathlib import Path
import subprocess
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mysql', default='mysql')
parser.add_argument('--port', type=int, required=True)
args = parser.parse_args()
client = [args.mysql, '--no-defaults', '--protocol=TCP', '--host=127.0.0.1',
          f'--port={args.port}', '--user=root', '--batch', '--skip-column-names']
sql_root = Path(__file__).resolve().parents[1] / 'data/sql/db-characters'
files = sorted(sql_root.rglob('*.sql'), key=lambda path: path.name)
base = (sql_root / 'base/custom_playerbots.sql').read_text(encoding='utf-8')
migration = next(path for path in files if 'autonomous' in path.name)


def sql(statement, database=None):
    command = client + ([database] if database else [])
    return subprocess.run(command, input=statement, text=True, encoding='utf-8',
                          capture_output=True, check=True).stdout.strip()


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def apply_all(database):
    # Match AzerothCore's filename ordering, deliberately NOT directory order.
    for path in files:
        sql(path.read_text(encoding='utf-8'), database)


for scenario in ('fresh', 'legacy', 'base_first', 'existing_on_off', 'old_update_applied'):
    database = 'cpb_test_' + uuid.uuid4().hex
    sql(f'CREATE DATABASE `{database}`')
    try:
        if scenario in ('legacy', 'old_update_applied'):
            legacy = '\n'.join(line for line in base.splitlines() if '`autonomous`' not in line)
            sql(legacy, database)
            sql("INSERT INTO custom_playerbots (guid, account_id, autologin, created_at) "
                "VALUES (1, 42, 0, '2026-01-01 00:00:00')", database)
            if scenario == 'old_update_applied':
                sql('ALTER TABLE custom_playerbots ADD COLUMN autonomous '
                    'TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER autologin', database)
                sql('UPDATE custom_playerbots SET autonomous=1 WHERE guid=1', database)
        elif scenario in ('base_first', 'existing_on_off'):
            sql(base, database)
            if scenario == 'existing_on_off':
                sql('INSERT INTO custom_playerbots (guid, account_id, autonomous) '
                    'VALUES (1, 42, 0), (2, 43, 1)', database)

        apply_all(database)
        definition = sql('SHOW CREATE TABLE custom_playerbots', database)
        rows = sql('SELECT * FROM custom_playerbots ORDER BY guid', database)
        apply_all(database)  # retry/reapplication must be safe and preserve data
        check(sql('SHOW CREATE TABLE custom_playerbots', database) == definition,
              scenario + ': repeat changed schema')
        check(sql('SELECT * FROM custom_playerbots ORDER BY guid', database) == rows,
              scenario + ': repeat changed roster')
        expected_default = '0' if scenario in ('legacy', 'old_update_applied') else '1'
        column_default = sql("SELECT COLUMN_DEFAULT FROM information_schema.COLUMNS "
                             "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='custom_playerbots' "
                             "AND COLUMN_NAME='autonomous'", database)
        check(column_default == expected_default, scenario + ': wrong default')
        if scenario in ('legacy', 'old_update_applied'):
            expected = '1' if scenario == 'old_update_applied' else '0'
            check(sql('SELECT autonomous FROM custom_playerbots WHERE guid=1', database) == expected,
                  scenario + ': changed autonomy')
            check(sql('SELECT account_id, autologin, created_at FROM custom_playerbots '
                      'WHERE guid=1', database) == '42\t0\t2026-01-01 00:00:00',
                  scenario + ': lost existing data')
        if scenario == 'existing_on_off':
            check(sql('SELECT autonomous FROM custom_playerbots ORDER BY guid', database) == '0\n1',
                  scenario + ': changed existing autonomy')
        print('PASS:', scenario, '(including replay and data preservation)')
    finally:
        # Only the unique database created by this iteration is removed.
        sql(f'DROP DATABASE `{database}`')

print('All five migration scenarios passed. First migration:', migration.name)
