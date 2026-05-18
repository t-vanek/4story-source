using System.Diagnostics.CodeAnalysis;
using System.Security.Cryptography;

namespace FourStory.Protocol.Crypto;

// MD5 + RC4 are intentional: we must speak the legacy wire protocol byte-for-byte.
// Modern crypto comes later when the client is rewritten.
[SuppressMessage("Security", "CA5351:Do Not Use Broken Cryptographic Algorithms",
    Justification = "Legacy protocol compatibility — see _rewrite/docs/PROTOCOL.md §2.")]
[SuppressMessage("Security", "CA5350:Do Not Use Weak Cryptographic Algorithms",
    Justification = "Legacy protocol compatibility — see _rewrite/docs/PROTOCOL.md §2.")]

/// <summary>
/// RC4 stream cipher used as the outer encryption layer for client→server traffic.
/// .NET dropped built-in RC4 (CALG_RC4) so this is a small hand-rolled implementation.
///
/// Key derivation matches Win32 CryptoAPI flow used in the legacy server
/// (<c>Server/TNetLib/CryptographyExt.cpp</c>):
/// <list type="number">
/// <item>MD5 the raw key bytes (39 bytes from <see cref="KeyTable.RawSecretKey"/>).</item>
/// <item>Use the first 16 bytes of that MD5 hash as the RC4 key (CALG_RC4 with CryptDeriveKey
/// from a CALG_MD5 hash uses the full hash unmodified for a 128-bit key).</item>
/// </list>
/// </summary>
public static class Rc4Layer
{
    /// <summary>
    /// The 16-byte RC4 key derived from <see cref="KeyTable.RawSecretKey"/>.
    /// Cached because MD5 is deterministic and the input never changes.
    /// </summary>
    public static byte[] DefaultKey { get; } = MD5.HashData(KeyTable.RawSecretKey);

    /// <summary>
    /// XOR the buffer in place with RC4 keystream. RC4 is symmetric, so the same call
    /// encrypts and decrypts.
    /// </summary>
    public static void TransformInPlace(Span<byte> data, ReadOnlySpan<byte> key)
    {
        if (key.IsEmpty)
        {
            throw new ArgumentException("RC4 key must not be empty.", nameof(key));
        }

        Span<byte> s = stackalloc byte[256];
        InitState(s, key);

        var i = 0;
        var j = 0;
        for (var idx = 0; idx < data.Length; idx++)
        {
            i = (i + 1) & 0xFF;
            j = (j + s[i]) & 0xFF;
            (s[i], s[j]) = (s[j], s[i]);
            var keyStreamByte = s[(s[i] + s[j]) & 0xFF];
            data[idx] ^= keyStreamByte;
        }
    }

    /// <summary>Encrypts/decrypts using the default key derived from <see cref="KeyTable.RawSecretKey"/>.</summary>
    public static void TransformInPlace(Span<byte> data) => TransformInPlace(data, DefaultKey);

    /// <summary>
    /// Transforms a complete packet (offset 0..end) but preserves the first two bytes
    /// (<c>wSize</c>) on the wire. Matches the asymmetric C++ behavior in
    /// <c>Session.cpp::Decrypt</c> where <c>EncryptBuffer(CALG_RC4, m_pBuf, wSize, ...)</c>
    /// is called over the whole packet and then <c>m_pHeader-&gt;m_wSize</c> is restored.
    ///
    /// Both sides (client encrypt and server decrypt) must use this helper so the RC4
    /// keystream alignment stays consistent.
    /// </summary>
    public static void TransformPacketPreservingWSize(Span<byte> packet, ReadOnlySpan<byte> key)
    {
        if (packet.Length < 2)
        {
            throw new ArgumentException("Packet must have at least 2 bytes (wSize).", nameof(packet));
        }
        var savedWSize0 = packet[0];
        var savedWSize1 = packet[1];
        TransformInPlace(packet, key);
        packet[0] = savedWSize0;
        packet[1] = savedWSize1;
    }

    /// <inheritdoc cref="TransformPacketPreservingWSize(Span{byte}, ReadOnlySpan{byte})"/>
    public static void TransformPacketPreservingWSize(Span<byte> packet) =>
        TransformPacketPreservingWSize(packet, DefaultKey);

    private static void InitState(Span<byte> s, ReadOnlySpan<byte> key)
    {
        for (var k = 0; k < 256; k++)
        {
            s[k] = (byte)k;
        }

        var j = 0;
        for (var k = 0; k < 256; k++)
        {
            j = (j + s[k] + key[k % key.Length]) & 0xFF;
            (s[k], s[j]) = (s[j], s[k]);
        }
    }
}
