using FourStory.Persistence;
using FourStory.Persistence.Game;
using FourStory.Persistence.Global;
using FourStory.Protocol;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Auth;

/// <summary>
/// Outcome of <see cref="CharService.CreateAsync"/>.
/// Values mirror legacy <c>TCREATECHAR_RESULT</c> in
/// <c>Lib/Own/TProtocol/include/NetCode.h:284-295</c>.
/// </summary>
public enum CreateCharResult : byte
{
    Success = 0,
    NoGroup = 1,
    DuplicateName = 2,
    InvalidSlot = 3,
    /// <summary>CR_PROTECTED — disallowed name (charset violation, reserved word, etc.).</summary>
    Protected = 4,
    /// <summary>CR_OVERCHAR — name length out of [3,16] or slot count exhausted.</summary>
    OverChar = 5,
    NeedCard = 6,
    InternalError = 7,
}

public enum DeleteCharResult : byte
{
    Success = 0,
    Failed = 1, // legacy: in-guild or not-owned-by-user
    InvalidPassword = 2, // re-auth fail (our addition)
}

public sealed record CreateCharOutcome(CreateCharResult Result, int CharId, byte RemainingSlots, byte Level);
public sealed record DeleteCharOutcome(DeleteCharResult Result, byte RemainingSlots);

/// <summary>
/// Character lifecycle (create/delete) ported from the legacy <c>TCreateChar</c> /
/// <c>TDeleteChar</c> stored procedures. Touches both DBs: TCHARTABLE in the game DB
/// is the per-world char data; TALLCHARTABLE in the global DB is the cross-world index.
/// </summary>
public sealed class CharService
{
    private readonly IDbContextFactory<GlobalDbContext> _globalFactory;
    private readonly IDbContextFactory<GameDbContext> _gameFactory;
    private readonly ILogger<CharService> _logger;

    public CharService(
        IDbContextFactory<GlobalDbContext> globalFactory,
        IDbContextFactory<GameDbContext> gameFactory,
        ILogger<CharService> logger)
    {
        _globalFactory = globalFactory;
        _gameFactory = gameFactory;
        _logger = logger;
    }

    public async Task<CreateCharOutcome> CreateAsync(
        int userId,
        byte groupId,
        byte slot,
        string name,
        byte cls,
        byte race,
        byte country,
        byte sex,
        byte hair,
        byte face,
        byte body,
        byte pants,
        byte hand,
        byte foot,
        byte startingLevel,
        bool inPcBang,
        CancellationToken ct)
    {
        // Name length & charset validation (CSHandler.cpp:1024-1067). Done before any DB I/O.
        if (name.Length < 3 || name.Length > ProtocolConstants.MaxNameLength)
        {
            return new CreateCharOutcome(CreateCharResult.OverChar, 0, 0, 0);
        }
        if (!IsValidCharName(name))
        {
            return new CreateCharOutcome(CreateCharResult.Protected, 0, 0, 0);
        }

        await using var game = await _gameFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        await using var global = await _globalFactory.CreateDbContextAsync(ct).ConfigureAwait(false);

        // ===== Validation (TCreateChar.sql:88-104, TGLOBAL_RAGEZONE/TCreateChar.sql:30-42) =====
        var nameTaken =
            await game.TCHARTABLEs.AnyAsync(c => c.szNAME == name, ct).ConfigureAwait(false) ||
            await game.TNPCCHARTs.AnyAsync(n => n.szName == name, ct).ConfigureAwait(false) ||
            await game.TMONSTERCHARTs.AnyAsync(m => m.szName == name, ct).ConfigureAwait(false) ||
            await global.TALLCHARTABLEs.AnyAsync(c => c.szName == name, ct).ConfigureAwait(false) ||
            await global.TKEEPINGNAMEs.AnyAsync(k => EF.Functions.Like(name, k.szName), ct).ConfigureAwait(false);
        if (nameTaken)
        {
            return new CreateCharOutcome(CreateCharResult.DuplicateName, 0, 0, 0);
        }

        // Reserved name: a row in TRESERVEDNAME locks the name to a specific dwUserID.
        // Same user → allowed; different user → reject as DuplicateName (matches the
        // legacy SP's RETURN 2 — see TGLOBAL_RAGEZONE/TCreateChar.sql:40-42).
        var reservedOwner = await global.TRESERVEDNAMEs
            .Where(r => r.szName == name)
            .Select(r => (int?)r.dwUserID)
            .FirstOrDefaultAsync(ct)
            .ConfigureAwait(false);
        if (reservedOwner is not null && reservedOwner != userId)
        {
            _logger.LogInformation(
                "CREATECHAR rejected: name '{Name}' reserved for user={ReservedFor}, requested by user={UserId}",
                name, reservedOwner, userId);
            return new CreateCharOutcome(CreateCharResult.DuplicateName, 0, 0, 0);
        }

        var slotInUse = await game.TCHARTABLEs
            .AnyAsync(c => c.dwUserID == userId && c.bSlot == slot && c.bDelete == 0, ct)
            .ConfigureAwait(false);
        if (slotInUse)
        {
            return new CreateCharOutcome(CreateCharResult.InvalidSlot, 0, 0, 0);
        }

        if (inPcBang)
        {
            // Legacy SP at TCreateChar.sql:36-38 had the PCBang creation guard commented
            // out, so we don't reject — just log so PCBang traffic is visible in audits.
            _logger.LogInformation("CREATECHAR from PCBang session (user={UserId}, world={World})", userId, groupId);
        }

        // ===== Starter defaults (TCreateChar.sql:79-138) =====
        // HP/MP base = 2 + class.wCON*7+1, 2 + race.wMEN*9+1. For MVP we hard-code
        // a sane starter (50 HP / 30 MP). Wire up TCLASSCHART/TRACECHART later.
        const int hp = 50, mp = 30;
        var level = startingLevel == 0 ? (byte)1 : startingLevel;
        const int exp = 1;
        const short skillPoint = 0;
        const float posX = 3664.405f, posY = 86.16578f, posZ = 557.2542f;
        const short dir = 762;
        const short mapId = 2010;
        const short spawnId = 15003;
        const byte oriCountry = 4;

        // Insert TCHARTABLE first to get dwCharID (identity).
        var now = DateTime.UtcNow;
        var charRow = new TCHARTABLE
        {
            dwUserID = userId,
            bSlot = slot,
            szNAME = name,
            bStartAct = 0,
            bClass = cls,
            bRace = race,
            bCountry = country,
            bOriCountry = oriCountry,
            bSex = sex,
            bRealSex = sex,
            bHair = hair,
            bFace = face,
            bBody = body,
            bPants = pants,
            bHand = hand,
            bFoot = foot,
            bHelmetHide = 0,
            bLevel = level,
            dwEXP = exp,
            dwHP = hp,
            dwMP = mp,
            wSkillPoint = skillPoint,
            dwRegion = 0,
            dwGold = 0,
            dwSilver = 0,
            dwCooper = 0,
            bGuildLeave = 0,
            dwGuildLeaveTime = 0,
            wMapID = mapId,
            wSpawnID = spawnId,
            wLastSpawnID = spawnId,
            wTemptedMon = 0,
            bAftermath = 0,
            fPosX = posX,
            fPosY = posY,
            fPosZ = posZ,
            wDIR = dir,
            dwRankPoint = 0,
            bDelete = 0,
            dCreateDate = now,
            dDeleteDate = null,
            dwLastDestination = 0,
            dLogoutDate = now,
        };
        game.TCHARTABLEs.Add(charRow);
        await game.SaveChangesAsync(ct).ConfigureAwait(false);

        // Mirror into the global index. (Skip the welcome mail, starter inventory,
        // start skills, hotkeys etc. — those belong to the gameplay phase, not the
        // wire-level create handshake.)
        var allRow = new TALLCHARTABLE
        {
            dwUserID = userId,
            bWorldID = groupId,
            dwCharID = charRow.dwCharID,
            bSlot = slot,
            szName = name,
            bClass = cls,
            bRace = race,
            bCountry = country,
            bSex = sex,
            bHair = hair,
            bFace = face,
            bBody = body,
            bPants = pants,
            bHand = hand,
            bFoot = foot,
            bLevel = level,
            dwEXP = exp,
            bDelete = 0,
            dCreateDate = now,
            dDeleteDate = null,
            dwPlayTime = 0,
            dwGold = 0,
            dwSilver = 0,
            dwCooper = 0,
        };
        global.TALLCHARTABLEs.Add(allRow);
        await global.SaveChangesAsync(ct).ConfigureAwait(false);

        var remaining = await CountActiveSlotsAsync(global, userId, ct).ConfigureAwait(false);
        _logger.LogInformation(
            "Char created: user={UserId} world={World} slot={Slot} name={Name} charId={CharId}",
            userId, groupId, slot, name, charRow.dwCharID);
        return new CreateCharOutcome(CreateCharResult.Success, charRow.dwCharID, remaining, level);
    }

    public async Task<DeleteCharOutcome> DeleteAsync(
        int userId,
        byte groupId,
        int charId,
        string password,
        CancellationToken ct)
    {
        // Legacy CSHandler.cpp:1228-1238: strPasswd.Length > MAX_NAME → DR_INVALIDPASSWD.
        // Defends against overlong inputs reaching BCrypt.
        if (password.Length > ProtocolConstants.MaxNameLength)
        {
            return new DeleteCharOutcome(DeleteCharResult.InvalidPassword, 0);
        }

        await using var global = await _globalFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        await using var game = await _gameFactory.CreateDbContextAsync(ct).ConfigureAwait(false);

        // Re-auth via password — legacy CS_DELCHAR_REQ carries strPasswd, but the SP
        // doesn't actually re-check. We DO check (defence in depth). Uses
        // PasswordHasher to support both BCrypt and legacy plaintext rows.
        var storedHash = await global.TACCOUNT_PWs
            .Where(a => a.dwUserID == userId)
            .Select(a => a.szPasswd)
            .FirstOrDefaultAsync(ct)
            .ConfigureAwait(false);
        if (storedHash is null || !PasswordHasher.Verify(password, storedHash))
        {
            return new DeleteCharOutcome(DeleteCharResult.InvalidPassword, 0);
        }

        var charRow = await game.TCHARTABLEs
            .FirstOrDefaultAsync(c => c.dwCharID == charId && c.dwUserID == userId && c.bDelete == 0, ct)
            .ConfigureAwait(false);
        if (charRow is null)
        {
            return new DeleteCharOutcome(DeleteCharResult.Failed, 0);
        }

        // Guild membership blocks delete (TDeleteChar.sql:38-42).
        var inGuild = await game.TGUILDMEMBERTABLEs
            .AnyAsync(m => m.dwCharID == charId, ct)
            .ConfigureAwait(false);
        if (inGuild)
        {
            return new DeleteCharOutcome(DeleteCharResult.Failed, 0);
        }

        // Soft delete for level > 5 (preserve audit trail); hard delete otherwise.
        if (charRow.bLevel > 5)
        {
            charRow.bDelete = 1;
            charRow.dDeleteDate = DateTime.UtcNow;
            var allRow = await global.TALLCHARTABLEs
                .FirstOrDefaultAsync(a => a.dwCharID == charId && a.bWorldID == groupId, ct)
                .ConfigureAwait(false);
            if (allRow is not null)
            {
                allRow.bDelete = 1;
                allRow.dDeleteDate = DateTime.UtcNow;
            }
        }
        else
        {
            game.TCHARTABLEs.Remove(charRow);
            await global.TALLCHARTABLEs
                .Where(a => a.dwCharID == charId && a.bWorldID == groupId)
                .ExecuteDeleteAsync(ct).ConfigureAwait(false);
            // Cascading deletes of inventory/skills/etc. are intentionally NOT performed here
            // — Phase 3 (gameplay) will own those tables. For MVP / no-data DB, the row removals
            // above are enough to make slot free.
        }

        await game.SaveChangesAsync(ct).ConfigureAwait(false);
        await global.SaveChangesAsync(ct).ConfigureAwait(false);

        var remaining = await CountActiveSlotsAsync(global, userId, ct).ConfigureAwait(false);
        _logger.LogInformation(
            "Char deleted: user={UserId} world={World} char={CharId} softDelete={Soft}",
            userId, groupId, charId, charRow.bLevel > 5);
        return new DeleteCharOutcome(DeleteCharResult.Success, remaining);
    }

    /// <summary>
    /// Legacy <c>CheckCharName</c>: name must be ASCII a-z A-Z 0-9 only.
    /// Reject anything with spaces, punctuation, or non-ASCII letters.
    /// </summary>
    private static bool IsValidCharName(string name)
    {
        foreach (var c in name)
        {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            {
                return false;
            }
        }
        return true;
    }

    private static async Task<byte> CountActiveSlotsAsync(GlobalDbContext db, int userId, CancellationToken ct)
    {
        var n = await db.TALLCHARTABLEs
            .CountAsync(c => c.dwUserID == userId && c.bDelete == 0, ct)
            .ConfigureAwait(false);
        return (byte)Math.Min(n, byte.MaxValue);
    }
}
