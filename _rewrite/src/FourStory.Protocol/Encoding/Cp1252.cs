using System.Text;

namespace FourStory.Protocol.Encoding;

/// <summary>
/// CP1252 (Windows-1252) is the wire encoding for all <c>STRING</c> fields in the legacy protocol
/// (confirmed via DB collation <c>Latin1_General_CI_AS_KS</c> on TGAME_RAGEZONE).
/// Single source of truth so consumers don't repeatedly call <c>Encoding.GetEncoding(1252)</c>.
/// </summary>
public static class Cp1252
{
    static Cp1252()
    {
        System.Text.Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        Instance = System.Text.Encoding.GetEncoding(1252);
    }

    public static System.Text.Encoding Instance { get; }
}
