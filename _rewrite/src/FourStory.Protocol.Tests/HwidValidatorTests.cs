using FourStory.Shared;

namespace FourStory.Protocol.Tests;

public class HwidValidatorTests
{
    [Fact]
    public void AllSegments_Concatenated_InOrder()
    {
        var v = new HwidValidator
        {
            WinDeviceSegment = "DEV-",
            WinSerialSegment = "SER-",
            WinUserSegment = "USR-",
            MoboSegment = "MOB-",
            CpuSegment = "CPU",
        };
        var result = v.TryComputeChecksum(
            HwidSegments.WinDevice | HwidSegments.WinSerial | HwidSegments.WinUser |
            HwidSegments.Mobo | HwidSegments.Cpu);
        Assert.Equal("DEV-SER-USR-MOB-CPU", result);
    }

    [Fact]
    public void MissingSegment_ReturnsNull()
    {
        var v = new HwidValidator
        {
            WinDeviceSegment = "DEV",
            // WinSerialSegment intentionally null
        };
        var result = v.TryComputeChecksum(HwidSegments.WinSerial);
        Assert.Null(result);
    }

    [Fact]
    public void WinDeviceAlwaysIncluded_LegacyBitmaskQuirk()
    {
        // HwidSegments.WinDevice == 0 → its flag-test always passes, mirroring C++.
        var v = new HwidValidator { WinDeviceSegment = "DEV", CpuSegment = "CPU" };
        var result = v.TryComputeChecksum(HwidSegments.Cpu);
        // Both WinDevice and Cpu segments included because WinDevice's flag is 0.
        Assert.Equal("DEVCPU", result);
    }
}
