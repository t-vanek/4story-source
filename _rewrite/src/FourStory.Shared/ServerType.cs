namespace FourStory.Shared;

/// <summary>
/// Server-role identifiers used by <c>TSERVER.bType</c>.
/// Mirrors the SVRGRP_* constants in
/// <c>Lib/Own/TProtocol/include/CTProtocol.h:30</c>. Only the values we
/// actually rely on are defined here; expand as more are confirmed against
/// the C++ header.
/// </summary>
public enum ServerType : byte
{
    /// <summary>SVRGRP_MAPSVR — the gameplay map server (what clients connect to after CS_START_ACK).</summary>
    MapSvr = 4,
}
