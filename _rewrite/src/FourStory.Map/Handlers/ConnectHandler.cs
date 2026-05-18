using FourStory.Map.Network;
using FourStory.Map;
using FourStory.Persistence;
using FourStory.Protocol;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.Map.Handlers;

/// <summary>
/// Validates the <c>dwKEY</c> handed off by TLoginSvr's CS_START_ACK against the
/// <c>TCURRENTUSER</c> row, then sends CS_CONNECT_ACK. Mirrors
/// <c>Server/TMapSvr/CSHandler.cpp::OnCS_CONNECT_REQ</c>.
///
/// CS_CONNECT_REQ payload (PROTOCOL.md §4c):
///   WORD wVersion, BYTE bChannel, DWORD dwUserID, DWORD dwCharID,
///   DWORD dwKEY, DWORD dwIPAddr, WORD wPort
///
/// CS_CONNECT_ACK payload:
///   BYTE bResult
///
/// On success this handler also notifies the WorldSvr via
/// <see cref="WorldClient.SendAddCharAckAsync"/> — same chain as the legacy
/// <c>SendMW_ADDCHAR_ACK</c> call at <c>CSHandler.cpp:388-393</c>. The packet
/// has fire-and-forget semantics here; the response (an eventual
/// <c>MW_ENTERSVR_REQ</c> from World, then <c>MW_CHARDATA_*</c> exchange) will
/// be handled by a separate dispatcher entry once those handlers land.
/// </summary>
public sealed class ConnectHandler
{
    private readonly IDbContextFactory<GlobalDbContext> _globalFactory;
    private readonly MapServerInfo _serverInfo;
    private readonly WorldClient _worldClient;
    private readonly ILogger<ConnectHandler> _logger;

    public ConnectHandler(
        IDbContextFactory<GlobalDbContext> globalFactory,
        MapServerInfo serverInfo,
        WorldClient worldClient,
        ILogger<ConnectHandler> logger)
    {
        _globalFactory = globalFactory;
        _serverInfo = serverInfo;
        _worldClient = worldClient;
        _logger = logger;
    }

    public void Register(MapPacketDispatcher dispatcher)
    {
        dispatcher.Register(MessageId.CS_CONNECT_REQ, OnConnectAsync);
    }

    private async ValueTask OnConnectAsync(MapConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        var r = new PacketReader(body.Span);
        var version = r.ReadUInt16();
        var channel = r.ReadByte();
        var userId = (int)r.ReadUInt32();
        var charId = (int)r.ReadUInt32();
        var key = r.ReadUInt32();
        var ipAddr = r.ReadUInt32();
        var port = r.ReadUInt16();

        var result = await ValidateAsync(userId, charId, key, channel, conn.LocalAddress, conn.LocalPort, ct).ConfigureAwait(false);

        if (result == ConnectResult.Ok)
        {
            conn.State.UserId = userId;
            conn.State.CharId = charId;
            conn.State.Key = key;
            conn.State.Channel = channel;
            conn.State.IpAddr = ipAddr;
            conn.State.Port = port;
            // Publish in the registry so MW_ENTERSVR_REQ (delivered later by World
            // via the WorldClient) can route back to this exact session. C++ uses
            // the global m_mapPlayer keyed by dwID for the same purpose
            // (Server/TMapSvr/SSHandler.cpp:3083 FindPlayer).
            conn.AssociateCharacter(charId);
        }

        _logger.LogInformation(
            "CONNECT_REQ user={UserId} char={CharId} key=0x{Key:X8} ch={Channel} ver=0x{Version:X4} -> {Result}",
            userId, charId, key, channel, version, result);

        Span<byte> ack = stackalloc byte[1];
        ack[0] = (byte)result;
        await conn.Session.SendAsync(MessageId.CS_CONNECT_ACK, ack, ct).ConfigureAwait(false);

        if (result == ConnectResult.Ok)
        {
            // Flip the codec to encrypted for subsequent gameplay packets — same activation
            // pattern as login. (PROTOCOL.md §5 Q4 — assumption pending verification with a
            // real client capture.)
            conn.Session.Codec.CryptEnabled = true;

            // Notify World that a character is connecting to this Map process so
            // it can allocate / find the TCHARACTER and kick off the bootstrap
            // exchange. Mirrors `SendMW_ADDCHAR_ACK(...)` at
            // Server/TMapSvr/CSHandler.cpp:388. We deliberately don't await the
            // response here — the legacy C++ code doesn't either; the
            // MW_ENTERSVR_REQ that comes back is dispatched asynchronously by
            // the SS handler chain. If the World session isn't currently up the
            // call will throw after SendReadyTimeout; we log and continue
            // (matching the legacy "drop the packet, log it" behaviour of
            // m_world.Say when the session is invalid). The client has already
            // received its successful CS_CONNECT_ACK at that point; the only
            // user-visible consequence is that CS_CONREADY_REQ → InitMap will
            // not have the character snapshot from World, which is the existing
            // ReadyHandler TODO.
            try
            {
                await _worldClient.SendAddCharAckAsync(
                    charId: (uint)charId,
                    key: key,
                    ipAddr: ipAddr,
                    port: port,
                    userId: (uint)userId,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex,
                    "MW_ADDCHAR_ACK to World failed for user={UserId} char={CharId} — character will not be registered on World side until next reconnect",
                    userId, charId);
            }
        }
    }

    private async Task<ConnectResult> ValidateAsync(
        int userId,
        int charId,
        uint key,
        byte channel,
        string localIp,
        ushort localPort,
        CancellationToken ct)
    {
        await using var db = await _globalFactory.CreateDbContextAsync(ct).ConfigureAwait(false);

        // Load the actual entity (not a projection) so we can write back below.
        var session = await db.TCURRENTUSERs
            .FirstOrDefaultAsync(s => s.dwKEY == (int)key && s.dwUserID == userId, ct)
            .ConfigureAwait(false);

        if (session is null)
        {
            return ConnectResult.InvalidKey;
        }
        if (session.bLocked != 0)
        {
            return ConnectResult.Locked;
        }

        // Stamp the row now that the client has chosen a specific character on a
        // specific channel. Mirrors the SP-side UPDATE done by TEnterServer
        // (Server/TMapSvr/DBAccess.h:5076-5103), which writes:
        //   dwKEY, dwUserID, dwCharID, bGroupID, bChannel, szIPAddr, wPort
        // dwKEY/dwUserID are already in the WHERE clause; the rest we stamp here.
        // szIPAddr / wPort are the Map server's own listening endpoint (the
        // address the client connected to), NOT the client's address. Other
        // servers use these to route hand-offs back to this Map process.
        // bGroupID identifies the world this Map process is serving.
        // dEnterDate is touched so the audit trail reflects the gameplay-side
        // entry, not just the login-side entry.
        session.dwCharID = charId;
        session.bChannel = channel;
        session.bGroupID = _serverInfo.GroupId;
        session.szIPAddr = localIp;
        session.wPort = (short)localPort;
        session.dEnterDate = DateTime.UtcNow;
        await db.SaveChangesAsync(ct).ConfigureAwait(false);

        return ConnectResult.Ok;
    }
}

/// <summary>Wire-level bResult values for CS_CONNECT_ACK.</summary>
public enum ConnectResult : byte
{
    Ok = 0,
    InvalidKey = 1,
    Locked = 2,
}
