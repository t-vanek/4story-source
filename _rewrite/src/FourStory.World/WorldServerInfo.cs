namespace FourStory.World;

/// <summary>
/// Static identity / configuration of this World server instance. In the
/// legacy C++ code these come from the registry under
/// <c>HKLM\SYSTEM\CurrentControlSet\Services\&lt;svc&gt;\Config</c>
/// (see <c>Server/TWorldSvr/TWorldSvr.cpp::LoadConfig</c>, lines 295-396):
/// <c>GroupID</c>, <c>ServerID</c>, <c>Port</c>, <c>DSN</c>, <c>DBUser</c>,
/// <c>DBPasswd</c>.
///
/// One World process serves one <c>TGROUP.bGroupID</c> (a "world"). It
/// listens on a single TCP port that the Map / Login / DM / Manager processes
/// for the same group connect to. Default port is <c>3815</c>
/// (<c>ProtocolConstants.DefaultWorldPort</c>, matches
/// <c>Lib/Own/TProtocol/include/CTProtocol.h::DEF_WORLDPORT</c>).
/// </summary>
public sealed record WorldServerInfo(byte GroupId, byte ServerId, ushort Port);
