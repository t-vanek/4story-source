using FourStory.Protocol.Crypto;

namespace FourStory.Protocol.Tests.Crypto;

public class Rc4LayerTests
{
    // Standard RC4 test vectors from https://datatracker.ietf.org/doc/html/rfc6229
    [Theory]
    [InlineData("0102030405", "b2396305f03dc027ccc3524a0a1118a8")]
    [InlineData("0102030405060708", "97ab8a1bf0afb96132f2f67258da15a8")]
    public void Rc4_MatchesRfc6229_FirstBytes(string keyHex, string expectedKeystreamHex)
    {
        var key = Convert.FromHexString(keyHex);
        var data = new byte[16]; // all zeros — RC4(zero) = keystream
        Rc4Layer.TransformInPlace(data, key);
        Assert.Equal(expectedKeystreamHex, Convert.ToHexString(data).ToLowerInvariant());
    }

    [Fact]
    public void Rc4_IsSymmetric()
    {
        var key = new byte[] { 1, 2, 3, 4, 5, 6, 7, 8 };
        var plaintext = "The quick brown fox jumps over the lazy dog"u8.ToArray();
        var buffer = (byte[])plaintext.Clone();

        Rc4Layer.TransformInPlace(buffer, key);
        Assert.NotEqual(plaintext, buffer);

        Rc4Layer.TransformInPlace(buffer, key);
        Assert.Equal(plaintext, buffer);
    }

    [Fact]
    public void DefaultKey_IsMd5OfSecretKey_16Bytes()
    {
        Assert.Equal(16, Rc4Layer.DefaultKey.Length);
        // Sanity: stable across calls.
        var k1 = Rc4Layer.DefaultKey;
        var k2 = Rc4Layer.DefaultKey;
        Assert.Same(k1, k2);
    }

    [Fact]
    public void RawSecretKey_Is39Bytes_MatchesC44StoryLayout()
    {
        // Length passed to CryptHashData was (38+1)*sizeof(TCHAR) = 39 in MBCS build.
        Assert.Equal(39, KeyTable.RawSecretKey.Length);
        // Last byte is NUL terminator.
        Assert.Equal((byte)0x00, KeyTable.RawSecretKey[^1]);
        // Curly quotes at expected positions.
        Assert.Equal((byte)0x92, KeyTable.RawSecretKey[22]);
        Assert.Equal((byte)0x94, KeyTable.RawSecretKey[30]);
    }
}
