namespace FourStory.Map;

/// <summary>
/// Static identity / configuration of this Map server instance — the values
/// stamped into <c>TCURRENTUSER</c> on CS_CONNECT and used everywhere the
/// gameplay code needs to know "which world am I serving".
///
/// In the legacy C++ code these come from the registry / INI: the Map server
/// is configured per-world (one process per <c>TGROUP.bGroupID</c>) and stamps
/// <c>szIPAddr</c>/<c>wPort</c> for the IP/port the client connected to so
/// other servers (notably TLoginSvr) can route hand-offs.
/// </summary>
/// <param name="GroupId">
/// World id this Map process serves. C++: registry <c>GroupID</c> →
/// <c>m_bGroupID</c> (Server/TMapSvr/TMapSvrModule.h).
/// </param>
/// <param name="ServerId">
/// Per-process Map server id, claimed in <c>MW_CONNECT_ACK</c> by stuffing the
/// low byte of <c>wServerID</c>. C++: <c>m_bServerID</c>, set from registry
/// (Server/TMapSvr/TMapSvr.cpp::LoadConfig). The Map cluster relies on this
/// being unique per Map process within a single World/group.
/// </param>
/// <param name="Port">TCP port this Map process listens on.</param>
public sealed record MapServerInfo(byte GroupId, byte ServerId, ushort Port);

/// <summary>
/// Endpoint of the WorldSvr this Map process should connect to as a client
/// (outbound SS link). C++: registry values <c>WorldIP</c> / <c>WorldPort</c>
/// loaded in <c>Server/TMapSvr/TMapSvr.cpp:683-745</c>, then used by
/// <c>m_world.Connect(m_szWorldIP, m_wWorldPort)</c> at line 949.
/// </summary>
/// <param name="Host">Hostname or dotted-quad IP of the WorldSvr.</param>
/// <param name="Port">TCP port of the WorldSvr (legacy default 3815).</param>
public sealed record WorldEndpoint(string Host, ushort Port);

/// <summary>
/// Channels this Map process hosts, advertised to the World server inside the
/// first SS packet (<c>MW_CONNECT_ACK</c>). C++: <c>m_mapTLOGCHANNEL</c> in
/// TMapSvr is populated by <c>TLOGCHANNEL</c> rows from <c>TGLOBAL.dbo.TCHANNEL</c>
/// at startup (Server/TMapSvr/DBAccess.cpp). For now we accept a static list
/// from config — channel orchestration will move into a service once the
/// channel data layer lands.
/// </summary>
public sealed record MapChannelList(IReadOnlyList<byte> Channels);
