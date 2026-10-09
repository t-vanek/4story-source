"""Independent native second-Map rejection probe, with read-only World monitor.

Synthetic account only. Proves failed native claim cannot erase an existing
World character; successful native replica admission is a later contract.
"""
import socket
import struct
import time
from verify_login_wire import frame, read_packet


def verify_map_rejection(conn, world_port, second_map_port, user, cid, key, connect_request):
    checks = []
    def check(ok, label):
        if not ok:
            raise RuntimeError('Map rejection wire: ' + label)
        checks.append(label)
    def exact(s, count):
        data = b''
        while len(data) < count:
            part = s.recv(count-len(data))
            if not part:
                raise RuntimeError('Map rejection monitor EOF')
            data += part
        return data
    def checksum(data):
        out = 0
        end = len(data)//4*4
        for i in range(0, end, 4):
            out ^= struct.unpack_from('<I', data, i)[0]
        for b in data[end:]:
            out ^= b
        return out
    def monitor():
        # CT_SERVICEMONITOR_ACK/REQ, SS header and DWORD tick/sessions/chars/users.
        with socket.create_connection(('127.0.0.1', world_port), timeout=3) as s:
            s.sendall(struct.pack('<HHII', 12, 0x931f, 17, 17))
            length, opcode, crc = struct.unpack('<HHI', exact(s, 8))
            if length != 24 or opcode != 0x931e:
                raise RuntimeError('Map rejection monitor unexpected frame')
            body = exact(s, 16)
            if checksum(body) != crc:
                raise RuntimeError('Map rejection monitor checksum')
            tick, _, chars, users = struct.unpack('<IIII', body)
            if tick != 17:
                raise RuntimeError('Map rejection monitor wrong tick')
            return chars, users
    original = conn.execute('SELECT server_id,owner_token,connection_id,phase FROM app_world.map_sessions WHERE user_id=%s', (user,)).fetchone()
    check(original and original[0] == 1 and original[3] == 'ready', 'primary native ownership is ready before second-Map attempt')
    check(monitor() == (1, 1), 'actual World contains the active primary character/account')
    with socket.create_connection(('127.0.0.1', second_map_port), timeout=8) as s:
        s.sendall(frame(connect_request(user, cid, key), 0x5281, 1))
        op, body = read_packet(s, 1)
        check(op == 0x5282 and body == bytes([2, 0]), 'ungranted second native Map returns exact CN_NOCHAR')
        check(s.recv(1) == b'', 'ungranted second native Map closes the rejected client')
    # Allow teardown's offloaded transaction and independent World link to run.
    deadline = time.monotonic()+1
    while time.monotonic() < deadline:
        if monitor() != (1, 1):
            raise RuntimeError('Map rejection wire: rejected second Map removed the valid World character')
        time.sleep(.025)
    check(True, 'rejected second native Map preserves the World character and active user')
    check(conn.execute('SELECT server_id,owner_token,connection_id,phase FROM app_world.map_sessions WHERE user_id=%s', (user,)).fetchone() == original,
          'rejected second Map leaves exact primary generation and phase unchanged')
    check(conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=%s AND "dwKEY"=%s', (user,key)).fetchone()[0] == 1,
          'rejected second Map keeps the current account reservation')
    return {'status':'passed','checks':checks,'scope':'Actual second native Map rejects absent grant while preserving primary PostgreSQL/World ownership; successful replica path not implemented'}
