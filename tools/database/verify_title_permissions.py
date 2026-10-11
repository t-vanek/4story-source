"""Exercise the deployed native Map role without touching any title rows."""

def verify_title_permissions(conn,role):
    import psycopg
    from psycopg import sql
    checks=[]
    with conn.transaction():
        conn.execute(sql.SQL('SET LOCAL ROLE {}').format(sql.Identifier(role)))
        allowed=conn.execute("SELECT has_table_privilege(current_user,'app_world.title_selection_operations','SELECT'),has_table_privilege(current_user,'app_world.title_selection_operations','INSERT'),has_sequence_privilege(current_user,'app_world.title_selection_operations_selection_id_seq','USAGE')").fetchone()
        if allowed!=(True,True,True):raise RuntimeError('Native title receipt read/append/identity grants missing')
        checks.append('Actual native Map role may read/append title receipts and allocate their identity')
        for statement,label in [
            ('DELETE FROM app_world.title_selection_operations WHERE false','Actual native Map role cannot delete title receipts'),
            ('UPDATE app_world.title_selection_operations SET selected_title=1 WHERE false','Actual native Map role cannot rewrite title receipts')]:
            try:
                with conn.transaction():conn.execute(statement)
            except psycopg.errors.InsufficientPrivilege:checks.append(label)
            else:raise RuntimeError(label)
    return {'status':'passed','checks':checks}
