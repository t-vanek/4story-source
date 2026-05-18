using System.Net.Sockets;
using FourStory.Login.Auth;
using FourStory.Login.Handlers;
using FourStory.Login.Services;
using FourStory.Persistence;
using FourStory.Protocol;
using FourStory.Shared;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.IntegrationTests;

[Collection("LoginServer")]
public class LoginFlowTests : IAsyncLifetime
{
    private const string TestUser = "testuser";
    private const string TestPassword = "testpass";
    private const string GlobalConnString =
        "Server=localhost;Database=TGLOBAL_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";
    private const string GameConnString =
        "Server=localhost;Database=TGAME_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";

    private IHost _host = null!;
    private int _port;

    public async Task InitializeAsync()
    {
        DebugLog.Clear();
        await ResetTestDataAsync();
        await ClearBansAsync(); // ensure no stale ban from a previous failed test
        _port = GetFreeTcpPort();
        var builder = Host.CreateApplicationBuilder([]);
        builder.Logging.SetMinimumLevel(LogLevel.Warning);

        builder.Services.AddDbContextFactory<GlobalDbContext>(opt => opt.UseSqlServer(GlobalConnString));
        builder.Services.AddDbContextFactory<GameDbContext>(opt => opt.UseSqlServer(GameConnString));
        builder.Services.AddSingleton<MapServerLocator>();
        builder.Services.AddSingleton<LoginRateLimiter>();
        builder.Services.AddSingleton<FourStory.Login.Services.ConnectionRegistry>();
        builder.Services.AddSingleton<IAuthService, AuthService>();
        builder.Services.AddSingleton<CharService>();
        builder.Services.AddSingleton<SessionTerminator>();
        builder.Services.AddSingleton<PacketDispatcher>(sp =>
        {
            var dispatcher = new PacketDispatcher(sp.GetRequiredService<ILogger<PacketDispatcher>>());
            var loginHandler = ActivatorUtilities.CreateInstance<LoginHandler>(sp);
            dispatcher.Register(MessageId.CS_LOGIN_REQ, loginHandler.HandleAsync);
            var lobbyHandlers = ActivatorUtilities.CreateInstance<LobbyHandlers>(sp);
            lobbyHandlers.Register(dispatcher);
            var terminate = ActivatorUtilities.CreateInstance<TerminateHandler>(sp);
            terminate.Register(dispatcher);
            return dispatcher;
        });
        var capturedPort = _port;
        builder.Services.AddHostedService(sp => new LoginServer(
            sp, sp.GetRequiredService<ILogger<LoginServer>>(), capturedPort));

        _host = builder.Build();
        await _host.StartAsync();
        // Give the listener a moment to bind.
        await Task.Delay(150);
    }

    public async Task DisposeAsync()
    {
        await _host.StopAsync();
        _host.Dispose();
    }

    private static int GetFreeTcpPort()
    {
        var l = new TcpListener(System.Net.IPAddress.Loopback, 0);
        l.Start();
        var port = ((System.Net.IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return port;
    }

    /// <summary>
    /// Each test run is idempotent: wipe TCURRENTUSER rows for the test user and any
    /// IntegTestX* characters so reruns don't fail with "duplicate session" / "name in use".
    /// </summary>
    private static async Task ResetTestDataAsync()
    {
        using var global = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await global.OpenAsync();
        using var cmd = global.CreateCommand();
        cmd.CommandText = """
            DELETE FROM TCURRENTUSER WHERE dwUserID IN (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID = 'testuser');
            DELETE FROM TALLCHARTABLE WHERE szName LIKE 'IntegTestX%';
            -- Ensure testuser has accepted the agreement so login succeeds (L2).
            -- Set bAgreement=1 explicitly; a separate test resets it to 0 to cover the AgreementRequired path.
            MERGE TUSERINFOTABLE AS t
            USING (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID = 'testuser') AS s
            ON t.dwUserID = s.dwUserID
            WHEN MATCHED THEN UPDATE SET bAgreement = 1
            WHEN NOT MATCHED THEN
                INSERT (dwUserID, bAgreement, bCanCreateCharCount, dCabinetUse)
                VALUES (s.dwUserID, 1, 6, '2008-01-01');
            """;
        await cmd.ExecuteNonQueryAsync();

        using var game = new Microsoft.Data.SqlClient.SqlConnection(GameConnString);
        await game.OpenAsync();
        using var cmd2 = game.CreateCommand();
        cmd2.CommandText = "DELETE FROM TCHARTABLE WHERE szNAME LIKE 'IntegTestX%';";
        await cmd2.ExecuteNonQueryAsync();
    }

    [Fact]
    public async Task FullLobbyFlow_LoginThroughStart_RoundTrips()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);

        // --- 1. CS_LOGIN_REQ → CS_LOGIN_ACK ---
        {
            await client.SendLoginRequestAsync(ProtocolConstants.Version, TestUser, TestPassword, cts.Token);
            var (_, ackBody) = await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
            var r = new PacketReader(ackBody);
            var result = (LoginResult)r.ReadByte();
            Assert.Equal(LoginResult.Success, result);
            var userId = r.ReadUInt32();
            Assert.True(userId > 0, "Expected non-zero userId in CS_LOGIN_ACK");

            // From this packet onward both sides encrypt (server flips m_bUseCrypt
            // after successful login — see LoginHandler.HandleAsync).
            client.CryptEnabled = true;
        }

        // --- 2. CS_GROUPLIST_REQ → CS_GROUPLIST_ACK ---
        {
            await client.SendAsync(MessageId.CS_GROUPLIST_REQ, ReadOnlyMemory<byte>.Empty, cts.Token);
            var (_, body) = await client.ReceiveAsync(MessageId.CS_GROUPLIST_ACK, cts.Token);
            var r = new PacketReader(body);
            var count = r.ReadByte();
            Assert.Equal(0, r.ReadByte()); // CheckFilePoint
            Assert.True(count >= 1, $"Expected ≥1 world, got {count}");
            // First group exists (Lapiris is bStatus=1 in test data).
            var name = r.ReadString();
            var groupId = r.ReadByte();
            Assert.NotEmpty(name);
            Assert.True(groupId > 0);
        }

        // --- 3. CS_CHANNELLIST_REQ(groupId=1) → CS_CHANNELLIST_ACK ---
        {
            Span<byte> req = stackalloc byte[1];
            req[0] = 1;
            await client.SendAsync(MessageId.CS_CHANNELLIST_REQ, req.ToArray(), cts.Token);
            var (_, body) = await client.ReceiveAsync(MessageId.CS_CHANNELLIST_ACK, cts.Token);
            var r = new PacketReader(body);
            var count = r.ReadByte();
            Assert.Equal(0, r.ReadByte()); // CheckFilePoint
            Assert.Equal(4, count); // Ch1, Ch2, Ch3, BR
        }

        // --- 4. CS_CHARLIST_REQ(groupId=1) → CS_CHARLIST_ACK ---
        {
            Span<byte> req = stackalloc byte[1];
            req[0] = 1;
            await client.SendAsync(MessageId.CS_CHARLIST_REQ, req.ToArray(), cts.Token);
            var (_, body) = await client.ReceiveAsync(MessageId.CS_CHARLIST_ACK, cts.Token);
            var r = new PacketReader(body);
            Assert.Equal(0, r.ReadByte()); // CheckFilePoint prelude (we don't ship exec-integrity)
            var count = r.ReadByte();
            // Test user has no characters yet — empty list.
            Assert.Equal(0, count);
        }

        // --- 5. CS_START_REQ → CS_START_ACK (no chars, but server still responds) ---
        {
            var body = new byte[6];
            var w = new PacketWriter(body);
            w.WriteByte(1); // groupId
            w.WriteByte(1); // channel
            w.WriteUInt32(0); // dwCharID — placeholder (no char created)
            await client.SendAsync(MessageId.CS_START_REQ, w.WrittenSpan[..w.Position].ToArray(), cts.Token);

            var (_, ackBody) = await client.ReceiveAsync(MessageId.CS_START_ACK, cts.Token);
            var r = new PacketReader(ackBody);
            var result = (LoginResult)r.ReadByte();
            Assert.Equal(LoginResult.Success, result);
            // dwMapIP, wPort, bServerID follow — verify the packet parses and the endpoint
            // is non-empty. The actual values come from MapServerLocator → TSERVER (the real
            // RaGEZONE seed maps group 1 to 192.168.1.155:5816 etc.), so we don't assert
            // specific constants — that would tie the test to seed data we don't own.
            var mapIp = r.ReadUInt32();
            var port = r.ReadUInt16();
            var serverId = r.ReadByte();
            Assert.NotEqual(0u, mapIp);
            Assert.NotEqual(0, port);
            Assert.NotEqual(0, serverId);
        }
    }

    [Fact]
    public async Task Login_WrongPassword_ReturnsInvalidPasswd()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);

        await client.SendLoginRequestAsync(ProtocolConstants.Version, TestUser, "wrong-password", cts.Token);
        var (_, ack) = await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
        var r = new PacketReader(ack);
        Assert.Equal(LoginResult.InvalidPasswd, (LoginResult)r.ReadByte());
    }

    [Fact]
    public async Task CreateChar_ThenList_ThenDelete_RoundTrips()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(20));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);

        // Log in first.
        await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
        client.CryptEnabled = true;
        await SelectGroupAsync(client, 1, cts.Token);

        var charName = $"IntegTestX{Guid.NewGuid():N}"[..16]; // szNAME is varchar(50), trim to 16 for readability.

        // --- CREATECHAR ---
        int newCharId;
        {
            var bodyBuf = new byte[64];
            var w = new PacketWriter(bodyBuf);
            w.WriteByte(1);                  // groupId
            w.WriteString(charName);
            w.WriteByte(0);                  // slot
            w.WriteByte(1);                  // class
            w.WriteByte(0);                  // race
            w.WriteByte(0);                  // country
            w.WriteByte(0);                  // sex
            w.WriteByte(1);                  // hair
            w.WriteByte(1);                  // face
            w.WriteByte(1);                  // body
            w.WriteByte(1);                  // pants
            w.WriteByte(1);                  // hand
            w.WriteByte(1);                  // foot
            w.WriteByte(0);                  // bLevelOption — 0 = no veteran bonus
            await client.SendAsync(MessageId.CS_CREATECHAR_REQ, w.WrittenSpan[..w.Position].ToArray(), cts.Token);

            var (_, ack) = await client.ReceiveAsync(MessageId.CS_CREATECHAR_ACK, cts.Token);
            var r = new PacketReader(ack);
            var result = r.ReadByte();
            Assert.Equal(0, result); // CreateCharResult.Success
            newCharId = (int)r.ReadUInt32();
            Assert.True(newCharId > 0, $"Expected positive charId, got {newCharId}");
            var ackName = r.ReadString();
            Assert.Equal(charName, ackName);
        }

        // --- CHARLIST should now include the new char ---
        {
            await client.SendAsync(MessageId.CS_CHARLIST_REQ, new byte[] { 1 }, cts.Token);
            var (_, body) = await client.ReceiveAsync(MessageId.CS_CHARLIST_ACK, cts.Token);
            var r = new PacketReader(body);
            Assert.Equal(0, r.ReadByte()); // CheckFilePoint
            var count = r.ReadByte();
            Assert.Equal(1, count);
            var id = (int)r.ReadUInt32();
            Assert.Equal(newCharId, id);
            var name = r.ReadString();
            Assert.Equal(charName, name);
        }

        // --- DELCHAR ---
        {
            var bodyBuf = new byte[64];
            var w = new PacketWriter(bodyBuf);
            w.WriteByte(1);                       // groupId
            w.WriteString(TestPassword);
            w.WriteUInt32((uint)newCharId);
            await client.SendAsync(MessageId.CS_DELCHAR_REQ, w.WrittenSpan[..w.Position].ToArray(), cts.Token);

            var (_, ack) = await client.ReceiveAsync(MessageId.CS_DELCHAR_ACK, cts.Token);
            var r = new PacketReader(ack);
            var result = r.ReadByte();
            Assert.Equal(0, result); // DeleteCharResult.Success
            var echoedCharId = (int)r.ReadUInt32();
            Assert.Equal(newCharId, echoedCharId);
        }

        // --- CHARLIST should be empty again ---
        {
            await client.SendAsync(MessageId.CS_CHARLIST_REQ, new byte[] { 1 }, cts.Token);
            var (_, body) = await client.ReceiveAsync(MessageId.CS_CHARLIST_ACK, cts.Token);
            var r = new PacketReader(body);
            Assert.Equal(0, r.ReadByte()); // CheckFilePoint
            Assert.Equal(0, r.ReadByte()); // bCount
        }
    }

    [Fact]
    public async Task DelChar_WrongPassword_ReturnsInvalidPassword()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
        client.CryptEnabled = true;
        await SelectGroupAsync(client, 1, cts.Token);

        var bodyBuf = new byte[64];
        var w = new PacketWriter(bodyBuf);
        w.WriteByte(1);
        w.WriteString("nope-wrong");
        w.WriteUInt32(99999);
        await client.SendAsync(MessageId.CS_DELCHAR_REQ, w.WrittenSpan[..w.Position].ToArray(), cts.Token);

        var (_, ack) = await client.ReceiveAsync(MessageId.CS_DELCHAR_ACK, cts.Token);
        var r = new PacketReader(ack);
        Assert.Equal((byte)2, r.ReadByte()); // DeleteCharResult.InvalidPassword
    }

    private static async Task LoginAsAsync(MockClient client, string user, string password, CancellationToken ct)
    {
        await client.SendLoginRequestAsync(ProtocolConstants.Version, user, password, ct);
        var (_, ack) = await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, ct);
        var r = new PacketReader(ack);
        var result = (LoginResult)r.ReadByte();
        Assert.Equal(LoginResult.Success, result);
    }

    /// <summary>
    /// Walks the lobby flow up to the point where CREATECHAR/DELCHAR is allowed:
    /// CHANNELLIST(groupId) → CHARLIST(groupId). This stamps the session's
    /// SelectedGroupId so subsequent character ops pass the groupId-consistency check.
    /// </summary>
    private static async Task SelectGroupAsync(MockClient client, byte groupId, CancellationToken ct)
    {
        await client.SendAsync(MessageId.CS_CHANNELLIST_REQ, new[] { groupId }, ct);
        await client.ReceiveAsync(MessageId.CS_CHANNELLIST_ACK, ct);
        await client.SendAsync(MessageId.CS_CHARLIST_REQ, new[] { groupId }, ct);
        await client.ReceiveAsync(MessageId.CS_CHARLIST_ACK, ct);
    }

    [Fact]
    public async Task Login_VersionMismatch_ReturnsVersionMismatch()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);

        await client.SendLoginRequestAsync(0xDEAD, TestUser, TestPassword, cts.Token);
        var (_, ack) = await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
        var r = new PacketReader(ack);
        Assert.Equal(LoginResult.VersionMismatch, (LoginResult)r.ReadByte());
    }

    [Fact]
    public async Task Login_AgreementNotAccepted_LoginSuccessButGroupListBlocked()
    {
        // Legacy treats LR_NEEDAGREEMENT as a successful login: LOGIN_ACK is Success, but
        // downstream lobby handlers (GROUPLIST, CHANNELLIST, CHARLIST, CREATECHAR, START)
        // reject silently until CS_AGREEMENT_REQ flips the in-memory flag.
        await SetAgreementAsync(0);
        try
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);

            await client.SendLoginRequestAsync(ProtocolConstants.Version, TestUser, TestPassword, cts.Token);
            var (_, ack) = await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
            Assert.Equal(LoginResult.Success, (LoginResult)new PacketReader(ack).ReadByte());
            client.CryptEnabled = true;

            // Sending CS_GROUPLIST_REQ now should be silently dropped (no ACK).
            await client.SendAsync(MessageId.CS_GROUPLIST_REQ, ReadOnlyMemory<byte>.Empty, cts.Token);
            var timeoutThrew = false;
            try
            {
                await client.ReceiveAsync(MessageId.CS_GROUPLIST_ACK, cts.Token, TimeSpan.FromSeconds(1));
            }
            catch (TimeoutException)
            {
                timeoutThrew = true;
            }
            Assert.True(timeoutThrew, "Expected GROUPLIST_REQ to be silently dropped before agreement");

            // Send CS_AGREEMENT_REQ to flip the flag, then GROUPLIST_REQ should succeed.
            var agreeBuf = new byte[2];
            new PacketWriter(agreeBuf).WriteUInt16(0x0001);
            await client.SendAsync(MessageId.CS_AGREEMENT_REQ, agreeBuf, cts.Token);
            await Task.Delay(200); // give the DB upsert time

            await client.SendAsync(MessageId.CS_GROUPLIST_REQ, ReadOnlyMemory<byte>.Empty, cts.Token);
            var (_, groupBody) = await client.ReceiveAsync(MessageId.CS_GROUPLIST_ACK, cts.Token);
            Assert.True(groupBody.Length >= 2); // bCount + CheckFilePoint at minimum
        }
        finally
        {
            await SetAgreementAsync(1);
        }
    }

    [Fact]
    public async Task Agreement_Accept_PersistsAndAllowsLogin()
    {
        // Start with bAgreement=0, then send AGREEMENT_REQ via a sneaky path:
        // login fails first → re-login after AGREEMENT_REQ should succeed.
        // CS_AGREEMENT_REQ requires an authenticated connection (RequireAuth gate), so
        // we have to drive it from a connection that already passed login. Easiest: accept
        // first while bAgreement=1, then verify the DB increments.
        await SetAgreementAsync(1);
        var before = await GetAgreementAsync();

        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
        client.CryptEnabled = true;

        var bodyBuf = new byte[2];
        var w = new PacketWriter(bodyBuf);
        w.WriteUInt16(0x0001); // agreement version
        await client.SendAsync(MessageId.CS_AGREEMENT_REQ, w.WrittenSpan[..w.Position].ToArray(), cts.Token);

        // No ACK — give the server a moment to write to the DB.
        await Task.Delay(300);
        var after = await GetAgreementAsync();
        Assert.Equal(before + 1, after);
    }

    [Fact]
    public async Task Veteran_ReturnsAllThreeLevels()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
        client.CryptEnabled = true;

        await client.SendAsync(MessageId.CS_VETERAN_REQ, ReadOnlyMemory<byte>.Empty, cts.Token);
        var (_, ack) = await client.ReceiveAsync(MessageId.CS_VETERAN_ACK, cts.Token);
        // Legacy wire: bOption + 3 level bytes = 4 bytes (CSSender.cpp:145-159).
        Assert.Equal(4, ack.Length);
        Assert.Equal(3, ack[0]); // bOption=3 = "all three available"
        // bytes 1-3 are level thresholds from TVETERANCHART; we don't assert exact values
        // because they come from seed data we don't own.
    }


    [Fact]
    public async Task CreateChar_ReservedNameForOtherUser_ReturnsDuplicateName()
    {
        var reservedName = $"IntegTestXR{Guid.NewGuid():N}"[..16];
        await AddReservedNameAsync(reservedName, ownerUserId: 999_999);
        try
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
            await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
            await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
            client.CryptEnabled = true;
            await SelectGroupAsync(client, 1, cts.Token);

            var buf = new byte[64];
            var w = new PacketWriter(buf);
            w.WriteByte(1);
            w.WriteString(reservedName);
            w.WriteByte(0);
            for (int i = 0; i < 11; i++)
            {
                w.WriteByte(1);
            }
            w.WriteByte(0); // bLevelOption
            await client.SendAsync(MessageId.CS_CREATECHAR_REQ, w.WrittenSpan[..w.Position].ToArray(), cts.Token);

            var (_, ack) = await client.ReceiveAsync(MessageId.CS_CREATECHAR_ACK, cts.Token);
            var r = new PacketReader(ack);
            Assert.Equal(2, r.ReadByte()); // DuplicateName
        }
        finally
        {
            await RemoveReservedNameAsync(reservedName);
        }
    }

    private static async Task AddReservedNameAsync(string name, int ownerUserId)
    {
        using var conn = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await conn.OpenAsync();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "INSERT INTO TRESERVEDNAME (dwUserID, szName) VALUES (@u, @n);";
        cmd.Parameters.AddWithValue("@u", ownerUserId);
        cmd.Parameters.AddWithValue("@n", name);
        await cmd.ExecuteNonQueryAsync();
    }

    private static async Task RemoveReservedNameAsync(string name)
    {
        using var conn = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await conn.OpenAsync();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "DELETE FROM TRESERVEDNAME WHERE szName = @n;";
        cmd.Parameters.AddWithValue("@n", name);
        await cmd.ExecuteNonQueryAsync();
    }

    [Fact]
    public async Task Login_RepeatedFailures_TriggersRateLimit()
    {
        // Drive enough wrong-password attempts to exceed the per-user threshold (5 in 1min).
        // After that, even a correct password should briefly return RateLimited.
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(20));
        const int burst = LoginRateLimiter.UserThreshold + 1;
        for (int i = 0; i < burst; i++)
        {
            await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
            await client.SendLoginRequestAsync(ProtocolConstants.Version, TestUser, "wrong-on-purpose", cts.Token);
            await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
        }

        // Final attempt — even with the correct password we should be locked out.
        await using var locked = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await locked.SendLoginRequestAsync(ProtocolConstants.Version, TestUser, TestPassword, cts.Token);
        var (_, ack) = await locked.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
        Assert.Equal(LoginResult.RateLimited, (LoginResult)new PacketReader(ack).ReadByte());
    }

    [Fact]
    public async Task Login_BannedAccount_ReturnsBanned()
    {
        // Insert a temporary TUSERPROTECTED row for testuser with an eternal ban.
        // The test cleans it up in `finally` even on failure.
        await AddBanAsync(eternal: true, durationDays: 0, reason: "integ-test-ban");
        try
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);

            await client.SendLoginRequestAsync(ProtocolConstants.Version, TestUser, TestPassword, cts.Token);
            var (_, ack) = await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
            var r = new PacketReader(ack);
            Assert.Equal(LoginResult.Banned, (LoginResult)r.ReadByte());
        }
        finally
        {
            await ClearBansAsync();
        }
    }

    private static async Task AddBanAsync(bool eternal, int durationDays, string reason)
    {
        using var conn = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await conn.OpenAsync();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            INSERT INTO TUSERPROTECTED
                (dwUserID, bBlockType, bEternal, bWorld, dwCharID, szCharName, startTime, dwDuration,
                 bBlockReason, szComment, szGMID, regDate, sentBanMail)
            SELECT TOP 1
                dwUserID, 1, @e, 1, 0, 'testuser', GETDATE(), @d,
                1, @r, 'integ-test-gm', GETDATE(), 0
            FROM TACCOUNT_PW WHERE szUserID = 'testuser';
            """;
        cmd.Parameters.AddWithValue("@e", eternal ? (byte)1 : (byte)0);
        cmd.Parameters.AddWithValue("@d", durationDays);
        cmd.Parameters.AddWithValue("@r", reason);
        await cmd.ExecuteNonQueryAsync();
    }

    private static async Task ClearBansAsync()
    {
        using var conn = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await conn.OpenAsync();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            DELETE FROM TUSERPROTECTED
            WHERE dwUserID IN (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID = 'testuser');
            """;
        await cmd.ExecuteNonQueryAsync();
    }

    [Fact]
    public async Task Login_DuplicateKicksOldSession()
    {
        // Two simultaneous logins for the same user: second receives LR_DUPLICATE; the first
        // should be force-closed (its TCP read loop exits).
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(20));
        await using var first = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await LoginAsAsync(first, TestUser, TestPassword, cts.Token);
        first.CryptEnabled = true;

        await using var second = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await second.SendLoginRequestAsync(ProtocolConstants.Version, TestUser, TestPassword, cts.Token);
        var (_, ack) = await second.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
        Assert.Equal(LoginResult.Duplicate, (LoginResult)new PacketReader(ack).ReadByte());

        // The first connection should now be terminated by the server. Try to send a packet —
        // either the send fails (socket closed) or a subsequent receive times out (no traffic).
        // Either way, we can't get a normal response on it.
        await Task.Delay(300);
        var firstStillResponsive = true;
        try
        {
            await first.SendAsync(MessageId.CS_HOTSEND_REQ, new byte[9], cts.Token);
            // HOTSEND has no ACK, so just observe whether the socket is still up. Try a request
            // that DOES respond — VETERAN_REQ.
            await first.SendAsync(MessageId.CS_VETERAN_REQ, ReadOnlyMemory<byte>.Empty, cts.Token);
            await first.ReceiveAsync(MessageId.CS_VETERAN_ACK, cts.Token, TimeSpan.FromSeconds(1));
        }
        catch
        {
            firstStillResponsive = false;
        }
        Assert.False(firstStillResponsive, "Expected first session to be force-closed after duplicate login");
    }

    [Fact]
    public async Task Start_LeavesCurrentUserRowForMapHandoff()
    {
        // After CS_START_ACK, the client is supposed to transition to MapSvr. Disconnecting
        // the Login session must NOT clear TCURRENTUSER (MapSvr would then reject the dwKEY).
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(20));
        await using (var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token))
        {
            await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
            client.CryptEnabled = true;
            await SelectGroupAsync(client, 1, cts.Token);

            var startBuf = new byte[6];
            var w = new PacketWriter(startBuf);
            w.WriteByte(1); w.WriteByte(1); w.WriteUInt32(0);
            await client.SendAsync(MessageId.CS_START_REQ, w.WrittenSpan[..w.Position].ToArray(), cts.Token);
            await client.ReceiveAsync(MessageId.CS_START_ACK, cts.Token);
        }
        await Task.Delay(300); // let DisposeAsync run on server side

        // Row should still be there — MapSvr will clean it up later.
        Assert.True(await CountCurrentUserRowsAsync() >= 1,
            "Expected TCURRENTUSER row to persist after START_ACK + disconnect (handoff to MapSvr)");
    }

    [Fact]
    public async Task Terminate_ClearsCurrentUserRow()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
        client.CryptEnabled = true;

        var rowsBefore = await CountCurrentUserRowsAsync();
        Assert.True(rowsBefore >= 1, "Expected a TCURRENTUSER row after login");

        var buf = new byte[4];
        var w = new PacketWriter(buf);
        w.WriteUInt32(ProtocolConstants.TerminateMagic);
        await client.SendAsync(MessageId.CS_TERMINATE_REQ, w.WrittenSpan.ToArray(), cts.Token);
        await Task.Delay(300); // no ACK; let the server reach SaveChangesAsync

        var rowsAfter = await CountCurrentUserRowsAsync();
        Assert.Equal(rowsBefore - 1, rowsAfter);
    }

    [Fact]
    public async Task Terminate_WrongMagic_IsIgnored()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);
        await LoginAsAsync(client, TestUser, TestPassword, cts.Token);
        client.CryptEnabled = true;

        var rowsBefore = await CountCurrentUserRowsAsync();
        var buf = new byte[4];
        var w = new PacketWriter(buf);
        w.WriteUInt32(0xDEADBEEF);
        await client.SendAsync(MessageId.CS_TERMINATE_REQ, w.WrittenSpan.ToArray(), cts.Token);
        await Task.Delay(300);

        // Magic mismatch → server logs warning and does NOT call SessionTerminator.
        // The row should still be there until the actual disconnect.
        Assert.Equal(rowsBefore, await CountCurrentUserRowsAsync());
    }

    private static async Task SetAgreementAsync(byte value)
    {
        using var conn = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await conn.OpenAsync();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            UPDATE TUSERINFOTABLE
            SET bAgreement = @v
            WHERE dwUserID IN (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID = 'testuser');
            """;
        cmd.Parameters.AddWithValue("@v", value);
        await cmd.ExecuteNonQueryAsync();
    }

    private static async Task<int> GetAgreementAsync()
    {
        using var conn = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await conn.OpenAsync();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            SELECT bAgreement FROM TUSERINFOTABLE
            WHERE dwUserID IN (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID = 'testuser');
            """;
        var v = await cmd.ExecuteScalarAsync();
        return v is null or DBNull ? 0 : Convert.ToInt32(v, System.Globalization.CultureInfo.InvariantCulture);
    }

    private static async Task<int> CountCurrentUserRowsAsync()
    {
        using var conn = new Microsoft.Data.SqlClient.SqlConnection(GlobalConnString);
        await conn.OpenAsync();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            SELECT COUNT(*) FROM TCURRENTUSER
            WHERE dwUserID IN (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID = 'testuser');
            """;
        return (int)(await cmd.ExecuteScalarAsync() ?? 0);
    }

    [Fact]
    public async Task Login_UnknownUser_ReturnsNoUser()
    {
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        await using var client = await MockClient.ConnectAsync("127.0.0.1", _port, cts.Token);

        await client.SendLoginRequestAsync(ProtocolConstants.Version, "nonexistent-user", "any", cts.Token);
        var (_, ack) = await client.ReceiveAsync(MessageId.CS_LOGIN_ACK, cts.Token);
        var r = new PacketReader(ack);
        Assert.Equal(LoginResult.NoUser, (LoginResult)r.ReadByte());
    }
}
