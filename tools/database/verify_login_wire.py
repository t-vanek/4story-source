"""Source-derived legacy Login frames over TCP, independent of the C++ codec.

This synthetic protocol peer is NOT an original-client compatibility certificate.
Constants/layout: Session.cpp, Packet.cpp, TNetSender.cpp, NetCode.h and
TUser::SendCS_LOGIN_ACK. No account provisioning or historical data access here.
"""
import hashlib
import socket
import struct
import time

MASK = (1 << 64) - 1
KEYS = (0x5193817ae183acee, 0x3891aeacbed18ead, 0x549aeced13de13a1,
        0x09aeb1498c1eade9, 0x19861acea1720ae7, 0x0139aecea89541a2, 0x6b97253c5fbb8b06)
SECRET = b"A5$$8AFS13A1::-11#!..'\x9219716AC&\x94/D1;;1#\x00"
CREDENTIAL = b'd7c9416b31ba5b02b27fe10c13ad3cbd8d9ad81d'


def rc4(data):
    key = hashlib.md5(SECRET).digest()
    state = list(range(256)); j = 0
    for i in range(256):
        j = (j + state[i] + key[i % len(key)]) & 255
        state[i], state[j] = state[j], state[i]
    out = bytearray(); i = j = 0
    for value in data:
        i = (i + 1) & 255; j = (j + state[i]) & 255
        state[i], state[j] = state[j], state[i]
        out.append(value ^ state[(state[i] + state[j]) & 255])
    return out


def checksum(body, key):
    result = crc = 0; full = len(body) // 8 * 8
    for i in range(0, full, 8):
        result ^= int.from_bytes(body[i:i + 8], 'little')
    for value in body[full:]:
        result ^= value
        crc = ((crc >> 4) & 0xffd) ^ key
        result = (result + crc) & MASK
    return result


def crypt_body(body, key):
    return bytes(value ^ ((key >> (8 * (i % 8))) & 255) for i, value in enumerate(body))


def frame(body, message=0x1988, sequence=1):
    key = KEYS[sequence % 7]; size = len(body) + 16
    head = bytearray(struct.pack('<HHIQ', size, message, sequence, checksum(body, key)))
    for i in range(14):
        head[i + 2] ^= (key + (size if i < 2 else message) + i) & 255
    encoded = rc4(head + crypt_body(body, key))
    encoded[:2] = struct.pack('<H', size)
    return encoded


def login_body(name=b'SyntheticWire', password=CREDENTIAL, version=0x2918):
    body = struct.pack('<H', version)
    for value in (b'', password, b'', b'', name):
        body += struct.pack('<i', len(value)) + value
    check = version * 2 - 500; count = check % 8; mix = check // 8
    for _ in range(count): check = ((check ^ mix) + 0x336c3aebf71a8b08) & MASK
    return body + struct.pack('<QQ', 0, check)


def receive(sock, count):
    value = b''
    while len(value) < count:
        part = sock.recv(count - len(value))
        if not part: raise RuntimeError('Unexpected close before complete Login reply')
        value += part
    return value


def read_packet(sock, expected_sequence=1):
    head = bytearray(receive(sock, 16)); key = KEYS[expected_sequence % 7]
    size = int.from_bytes(head[:2], 'little')
    if not 16 <= size < 65535: raise RuntimeError('Invalid reply frame size')
    for i in range(14):
        message = int.from_bytes(head[2:4], 'little')
        head[i + 2] ^= (key + (size if i < 2 else message) + i) & 255
    _, message, sequence, expected = struct.unpack('<HHIQ', head)
    body = crypt_body(receive(sock, size - 16), key)
    if sequence != expected_sequence or checksum(body, key) != expected:
        raise RuntimeError('Reply sequence or checksum differs from source')
    return message, body


def ack(sock, sequence=1):
    message, body = read_packet(sock, sequence)
    if message != 0x1989 or len(body) != 41:
        raise RuntimeError('Login ACK opcode or 41-byte layout differs from source')
    return struct.unpack('<BIIIIHBBIqq', body)


def verify_login_wire(host, port):
    checks = []
    def check(ok, label):
        if not ok: raise RuntimeError('Wire verification failed: ' + label)
        checks.append(label)
    def connect(body, fragmented=False, pipelined=False):
        sock = socket.create_connection((host, port), timeout=5)
        packet = frame(body)
        if fragmented:
            sock.sendall(packet[:1]); sock.sendall(packet[1:7]); sock.sendall(packet[7:])
        else: sock.sendall(packet)
        if pipelined: sock.sendall(frame(b"", message=0x198a, sequence=2))
        return sock
    for name, password, version, result, label in (
        (b'AbsentSynthetic', CREDENTIAL, 0x2918, 1, 'unknown account result 1'),
        (b'SyntheticWire', b'incorrect', 0x2918, 2, 'wrong credential result 2'),
        (b'SyntheticWire', CREDENTIAL, 0x2917, 4, 'unsupported version result 4'),
        (b'Synthetic105', CREDENTIAL, 0x2918, 7, 'backed-up account ban result 7'),
        (b'SyntheticAgreement', CREDENTIAL, 0x2918, 8, 'agreement result 8')):
        with connect(login_body(name, password, version)) as sock:
            check(ack(sock)[0] == result, label)
    with connect(login_body()) as limited:
        check(ack(limited)[0] == 5, 'rate limit uses original internal-error result 5')
    # Exercise the actual default limiter instead of disabling it for the test.
    time.sleep(10.1)
    first = connect(login_body(), fragmented=True, pipelined=True)
    try:
        reply = ack(first)
        check(reply[0] == 0 and reply[1] == 201 and reply[2] == 0 and reply[3] != 0 and reply[6] == 6,
              'fragmented RC4/MD5+XOR login returns account, fresh key and six slots')
        check(abs(reply[9] - int(time.time())) < 10, 'ACK time is current Unix seconds')
        message, body = read_packet(first, 2)
        check(message == 0x198b and body == bytes(5), 'pipelined group request waits for authentication and follows Login ACK')
        time.sleep(10.1)
        with connect(login_body()) as duplicate:
            check(ack(duplicate)[0] == 3, 'duplicate reply 3 reaches socket before close')
            check(duplicate.recv(1) == b'', 'duplicate requester closes after ACK')
        check(first.recv(1) == b'', 'original login socket also closes on duplicate')
    finally: first.close()
    # Cleanup is asynchronous; bounded retries observe it without sharing DB credentials.
    time.sleep(10.1)
    deadline = time.monotonic() + 5
    while True:
        with connect(login_body()) as retry:
            reply = ack(retry)
            if reply[0] == 0:
                check(reply[1] == 201, 'relogin succeeds after duplicate cleanup')
                break
            if reply[0] != 3 or time.monotonic() > deadline: raise RuntimeError('Duplicate cleanup did not complete')
        time.sleep(0.05)
    time.sleep(10.1)
    body = bytearray(login_body()); body[-1] ^= 1
    with connect(body) as corrupt:
        check(corrupt.recv(1) == b'', 'bad login checksum closes connection')
    time.sleep(10.1)
    with connect(login_body(b'SyntheticAbandoned')) as abandoned:
        # The fixture delays its audit inside the production login transaction.
        abandoned.shutdown(socket.SHUT_RDWR)
    time.sleep(1)
    checks.append('disconnect sent while fixture login transaction was delayed; database cleanup checked by harness')
    return {'scope': 'synthetic source-derived peer, original executable not tested',
            'client_encryption': 'RC4/MD5 over XOR frames, legacy secret includes NUL',
            'checks': checks, 'status': 'passed'}
