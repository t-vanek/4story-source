namespace FourStory.Shared;

/// <summary>
/// Service / session error codes. Port of <c>Server/TNetLib/ErrorCode.h</c>.
/// Layout matches legacy: high byte is the category, low bytes are the specific code.
/// </summary>
public enum ErrorCode : uint
{
    NoError = 0x00000000,

    // ===== Init-service errors (0x0100_00xx) =====
    InitOpenReg = 0x01000000,
    InitPortNotAssigned = 0x01000001,
    InitPasswdNotAssigned = 0x01000002,
    InitDsnNotAssigned = 0x01000003,
    InitSvrIdNotAssigned = 0x01000004,
    InitDbOpenFailed = 0x01000005,
    InitPrepareQuery = 0x01000006,
    InitResumeThread = 0x01000007,
    InitCreateThread = 0x01000008,
    InitSockLibFailed = 0x01000009,
    InitInvalidSockLib = 0x0100000A,
    InitCreateIocp = 0x0100000B,
    InitListenFailed = 0x0100000C,
    InitWaitForConnect = 0x0100000D,
    InitWorldIpNotAssigned = 0x0100000E,
    InitWorldPortNotAssigned = 0x0100000F,
    InitConnectWorld = 0x01000010,
    InitWaitForMsg = 0x01000011,
    InitUdpSocketFailed = 0x01000012,
    InitCtrlSvrData = 0x01000013,
    InitRelaySvrData = 0x01000014,
    InitLimitedLevel = 0x01000015,
    InitNation = 0x01000016,
    InitNProtect = 0x01000017,

    // ===== Session errors (0x0200_00xx) =====
    SessionInvalidMsg = 0x02000000,
    SessionInvalidChannel = 0x02000001,
    SessionInvalidChar = 0x02000002,
    SessionInvalidMap = 0x02000003,
    SessionDupServerId = 0x02000004,
    SessionExit = 0x02000005,
}

/// <summary>
/// Return values from the legacy <c>TLogin</c> stored procedure (see proc source at
/// <c>_rewrite/docs/schema/procs/TGLOBAL_RAGEZONE/TLogin.sql</c>). The C# auth service
/// produces the same wire-level <see cref="MessageId.CS_LOGIN_ACK"/>.<c>bResult</c> byte.
/// </summary>
public enum LoginResult : byte
{
    Success = 0,
    NoUser = 1,
    InvalidPasswd = 2,
    Duplicate = 3,
    /// <summary>Generic internal failure (LR_INTERNAL — SP failure, length overflow, etc.).</summary>
    InternalError = 4,
    /// <summary>Server returned bIPCheck=6 → reserved for a Korean PCBang rule. Forwarded verbatim.</summary>
    PcBangCheck = 6,
    /// <summary>Account is in TUSERPROTECTED (banned or under temporary block) — see ban-reason payload.</summary>
    Banned = 7,
    /// <summary>TUSERINFOTABLE.bAgreement = 0 — the user must accept the current agreement before login completes.</summary>
    AgreementRequired = 8,
    /// <summary>Wire protocol version doesn't match. Not in legacy SP — we needed a distinct code.</summary>
    VersionMismatch = 100,
    /// <summary>Too many failed attempts from this IP / for this account.</summary>
    RateLimited = 101,
}
