namespace FourStory.Protocol.Crypto;

public static class KeyTable
{
    public const int Count = 7;

    public static ReadOnlySpan<long> Keys =>
    [
        unchecked((long)0x5193817ae183aceeUL),
        unchecked((long)0x3891aeacbed18eadUL),
        unchecked((long)0x549aeced13de13a1UL),
        unchecked((long)0x09aeb1498c1eade9UL),
        unchecked((long)0x19861acea1720ae7UL),
        unchecked((long)0x0139aecea89541a2UL),
        unchecked((long)0x6b97253c5fbb8b06UL),
    ];

    public static long KeyFor(uint sequenceNumber) => Keys[(int)(sequenceNumber % Count)];

    // From Server/TNetLib/Session.cpp:16:
    //   CString g_strSecretKey = "A5$$8AFS13A1::-11#!..'’""1716AC&”""/D1;;1#";
    // 39 bytes including trailing NUL (length 38 + 1) when interpreted as CP1252.
    // The curly quotes ’ (0x92) and ” (0x94) are CP1252 single bytes.
    // RC4 key derivation = MD5 of the hash data passed by CryptHashData,
    // which is (GetLength() + 1) * sizeof(TCHAR) = 39 bytes.
    public static ReadOnlySpan<byte> RawSecretKey =>
    [
        0x41, 0x35, 0x24, 0x24, 0x38, 0x41, 0x46, 0x53,
        0x31, 0x33, 0x41, 0x31, 0x3A, 0x3A, 0x2D, 0x31,
        0x31, 0x23, 0x21, 0x2E, 0x2E, 0x27, 0x92, 0x31,
        0x37, 0x31, 0x36, 0x41, 0x43, 0x26, 0x94, 0x2F,
        0x44, 0x31, 0x3B, 0x3B, 0x31, 0x23, 0x00,
    ];
}
