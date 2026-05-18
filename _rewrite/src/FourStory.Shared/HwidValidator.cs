using System.Text;

namespace FourStory.Shared;

/// <summary>
/// Flags selecting which HWID segments to include when computing the per-account checksum.
/// Values match the legacy <c>Server/TLoginSvr/HwidManagerSvr.h</c>.
///
/// Note the legacy quirk: <see cref="WinDevice"/> = 0, which makes the corresponding
/// "and-mask" check always true. As a result WinDevice is always included regardless of
/// the requested flags. Preserved for wire compatibility.
/// </summary>
[Flags]
public enum HwidSegments
{
    WinDevice = 0,
    WinSerial = 1,
    WinUser = 2,
    Mobo = 4,
    Cpu = 8,
}

/// <summary>
/// Concatenates HWID segment strings sent by the client into a single checksum value.
/// Port of <c>HwidManagerSvr::GetSegmentChecksum</c>.
/// </summary>
public sealed class HwidValidator
{
    public string? WinDeviceSegment { get; set; }
    public string? WinSerialSegment { get; set; }
    public string? WinUserSegment { get; set; }
    public string? MoboSegment { get; set; }
    public string? CpuSegment { get; set; }

    /// <summary>
    /// Returns the concatenated checksum string, or <c>null</c> if any required
    /// segment is missing. Matches the legacy <c>BOOL</c> return: false ⇒ null here.
    /// </summary>
    public string? TryComputeChecksum(HwidSegments segments)
    {
        // Legacy guard: if hwidParams == 0 → return FALSE. Preserve.
        if (segments == 0 && !FlagSet(segments, HwidSegments.WinDevice))
        {
            // Unreachable because WinDevice==0; but kept literal so any future renumber
            // doesn't silently change behavior.
            return null;
        }

        var sb = new StringBuilder();

        // Note: HwidSegments.WinDevice is 0 so this check is always true.
        if (FlagSet(segments, HwidSegments.WinDevice))
        {
            if (string.IsNullOrEmpty(WinDeviceSegment))
            {
                return null;
            }
            sb.Append(WinDeviceSegment);
        }
        if (FlagSet(segments, HwidSegments.WinSerial))
        {
            if (string.IsNullOrEmpty(WinSerialSegment))
            {
                return null;
            }
            sb.Append(WinSerialSegment);
        }
        if (FlagSet(segments, HwidSegments.WinUser))
        {
            if (string.IsNullOrEmpty(WinUserSegment))
            {
                return null;
            }
            sb.Append(WinUserSegment);
        }
        if (FlagSet(segments, HwidSegments.Mobo))
        {
            if (string.IsNullOrEmpty(MoboSegment))
            {
                return null;
            }
            sb.Append(MoboSegment);
        }
        if (FlagSet(segments, HwidSegments.Cpu))
        {
            if (string.IsNullOrEmpty(CpuSegment))
            {
                return null;
            }
            sb.Append(CpuSegment);
        }

        return sb.ToString();
    }

    // Legacy bitmask test: (params & flag) == flag.
    // For flag = 0 (WinDevice) this is always true, matching C++ behavior.
    private static bool FlagSet(HwidSegments value, HwidSegments flag) =>
        ((int)value & (int)flag) == (int)flag;
}
