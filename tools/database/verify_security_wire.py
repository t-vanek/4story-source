"""Actual Login daemon email-code flow, using isolated SMTP and synthetic accounts.

Source client confirms then sends a fresh LOGIN with the form password. This peer
implements the source layout independently; it is not an original-client test.
Captured messages, codes and private challenge tokens never enter the report.
"""
import json
import re
import socket
import struct
import time
from verify_login_wire import frame, login_body, read_packet, ack

PASSWORD = b'4Story-Synthetic-Security!'
CREDENTIAL = b'6444b65498b4c2fc6c3ed2e16cdf93ea67e34dab'


def verify_security_wire(host, port, spool, conn):
    checks = []
    used = set()
    last_login = time.monotonic()
    def pace():
        nonlocal last_login
        # Actual bucket replenishes one attempt per ten seconds (burst five).
        time.sleep(max(0, 10.1 - (time.monotonic() - last_login)))
        last_login = time.monotonic()
    def check(ok, label):
        if not ok: raise RuntimeError('Security wire verification failed: ' + label)
        checks.append(label)
    def count(table, uid):
        # Table names are fixed fixture constants, user IDs are bound values.
        key = 'user_id' if table.startswith('login_security') else '"dwUserID"'
        return conn.execute(f'SELECT count(*) FROM app_global."{table}" WHERE {key}=%s', (uid,)).fetchone()[0]
    def await_clean(uid):
        deadline = time.monotonic() + 5
        while count('login_security_challenge', uid) or count('TCURRENTUSER', uid):
            if time.monotonic() > deadline: raise RuntimeError('Pending connection cleanup timed out')
            time.sleep(.025)
    def connect(uid, password=CREDENTIAL):
        pace()
        sock = socket.create_connection((host, port), timeout=5)
        sock.sendall(frame(login_body(f'SyntheticSecurity{uid}'.encode(), password)))
        return sock
    def challenge(uid):
        sock = connect(uid)
        try:
            check(read_packet(sock) == (0x199f, b''), 'LOGIN asks for code with original empty SECURITYCONFIRM packet')
            deadline = time.monotonic() + 5
            while True:
                for path in sorted(spool.glob('*.json')):
                    if path in used: continue
                    item = json.loads(path.read_text())
                    if item['recipient'] != f'synthetic{uid}@example.invalid': continue
                    used.add(path)
                    code = re.search(r'^    ([A-Z0-9]{6})$', item['message'], re.MULTILINE)
                    if not code: raise RuntimeError('Fixture mail lacks source-format code')
                    return sock, code.group(1).encode()
                if time.monotonic() > deadline: raise RuntimeError('Private SMTP capture timed out')
                time.sleep(.025)
        except Exception:
            sock.close(); raise
    def confirm(sock, code, seq=2, reply_seq=2, result=0):
        sock.sendall(frame(struct.pack('<i', len(code)) + code, message=0x19a0, sequence=seq))
        check(read_packet(sock, reply_seq) == (0x19a7, bytes([result])), 'code confirmation returns original one-byte SECURITYRESULT')
    def retry(sock, uid, seq=3, reply_seq=3, result=0, pipeline=False):
        pace()
        packet = frame(login_body(f'SyntheticSecurity{uid}'.encode(), PASSWORD), sequence=seq)
        if pipeline: packet += frame(b'', message=0x198a, sequence=seq+1)
        sock.sendall(packet)
        value = ack(sock, reply_seq)
        check(value[0] == result, 'client-driven LOGIN retry returns expected full 41-byte ACK')
        return value
    with connect(206, PASSWORD) as plain:
        check(ack(plain)[0] == 2, 'ordinary LOGIN rejects unhashed form password')
    sock, code = challenge(206)
    with sock:
        confirm(sock, code)
        check(count('TCURRENTUSER',206)==0 and count('TLOG',206)==0,
              'correct code alone creates neither current session nor login audit')
        sock.settimeout(.25)
        try: extra=sock.recv(1)
        except TimeoutError: extra=None
        finally: sock.settimeout(5)
        check(extra is None, 'no unsolicited LOGIN ACK after CODE_CORRECT')
        value=retry(sock,206,pipeline=True)
        check(value[1]==206 and value[3] and value[6]==6,
              'source raw-password retry preserves user, session key and character slots')
        check(read_packet(sock,4)==(0x198b,bytes(5)), 'pipelined lobby request follows completed verified LOGIN')
    await_clean(206)
    sock, code = challenge(206)
    sock.close(); await_clean(206)
    check(conn.execute('SELECT count(*) FROM app_global."TUSERTRUSTEDIP"').fetchone()[0]==0,
          'same-IP reconnect requires a new code and creates no persistent IP trust')

    sock, code=challenge(207)
    with sock:
        confirm(sock,b'!!!!!!',result=1)
        confirm(sock,code.lower(),seq=3,reply_seq=3)
        retry(sock,207,seq=4,reply_seq=4)
    await_clean(207)
    sock, code=challenge(208)
    with sock:
        for seq in range(2,7): confirm(sock,b'!!!!!!',seq=seq,reply_seq=seq,result=1)
        check(sock.recv(1)==b'', 'fifth wrong code flushes failure reply then closes connection')
    await_clean(208)

    first, first_code=challenge(209)
    second, second_code=challenge(209)
    try:
        check(count('login_security_challenge',209)==2, 'same account keeps two independent pending connections')
        second.close()
        confirm(first,first_code)
        retry(first,209)
        third, third_code=challenge(209)
        with third:
            confirm(third,third_code)
            retry(third,209,result=3)
            check(third.recv(1)==b'', 'verified duplicate receives ACK before its pending socket closes')
            check(first.recv(1)==b'', 'verified duplicate also closes original authenticated socket')
    finally: first.close(); second.close()
    await_clean(209)
    check(count('TLOG',209)==1, 'pending disconnect does not cancel another connection or create an extra session')

    sock, code=challenge(210)
    with sock:
        confirm(sock,code)
        retry(sock,211,result=5)
        check(sock.recv(1)==b'', 'account substitution after confirmation is refused and closes connection')
    await_clean(210)
    check(count('TCURRENTUSER',211)==0, 'foreign account receives no session from another connection grant')

    conn.execute('UPDATE app_global."TUSERINFOTABLE" SET "bAgreement"=0 WHERE "dwUserID"=212')
    sock, code=challenge(212)
    with sock:
        sock.sendall(frame(struct.pack('<H',0x2918),message=0x199a,sequence=2))
        check(sock.recv(1)==b'', 'pending challenge cannot submit account agreement')
    await_clean(212)
    check(conn.execute('SELECT "bAgreement" FROM app_global."TUSERINFOTABLE" WHERE "dwUserID"=212').fetchone()[0]==0,
          'unauthenticated agreement packet leaves persistent account state unchanged')
    sock, code=challenge(213)
    with sock:
        confirm(sock,code)
        conn.execute('UPDATE app_global."TUSERINFOTABLE" SET "bAgreement"=0 WHERE "dwUserID"=213')
        retry(sock,213,result=8)
    await_clean(213)

    sock, code=challenge(214)
    with sock:
        confirm(sock,code)
        conn.execute('INSERT INTO app_global."TUSERPROTECTED" ("dwUserID","bBlockType","bEternal","startTime","dwDuration","bBlockReason","szComment","szGMID","sentBanMail") VALUES (214,1,1,CURRENT_TIMESTAMP,0,0,\'synthetic\',\'test\',0)')
        retry(sock,214,result=7)
        check(sock.recv(1)==b'', 'ban added after confirmation blocks LOGIN and revokes pending connection')
    await_clean(214)

    for _ in range(3):
        sock, code=challenge(215); sock.close(); await_clean(215)
    with connect(215) as fourth:
        check(ack(fourth)[0]==5, 'disconnect cannot reset account email issuance budget')

    sock, code=challenge(216)
    with sock:
        conn.execute("UPDATE app_global.login_security_challenge SET issued_at=CURRENT_TIMESTAMP-interval '6 minutes', expires_at=CURRENT_TIMESTAMP-interval '1 minute' WHERE user_id=216")
        confirm(sock,code,result=1)
        check(sock.recv(1)==b'', 'expired database challenge refuses correct code and closes connection')
    await_clean(216)
    return {'status':'passed','scope':'synthetic source-derived encrypted peer and private SMTP sink; original executable not tested',
            'checks':checks,'mail_messages_captured':len(used),'external_mail_sent':False}
