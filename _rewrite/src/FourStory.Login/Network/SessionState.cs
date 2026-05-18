namespace FourStory.Login.Handlers;

/// <summary>
/// Per-connection state populated by handlers as the client progresses through
/// the lobby flow (login → group → channel → char → start).
/// Replaces the per-session fields scattered across <c>CTUser</c> / <c>CTLoginSession</c> in legacy.
/// </summary>
public sealed class SessionState
{
    /// <summary>Set by <see cref="LoginHandler"/> after successful auth.</summary>
    public int? UserId { get; set; }

    /// <summary>Session token issued by login. Equal to <c>TCURRENTUSER.dwKEY</c>.</summary>
    public uint? Key { get; set; }

    /// <summary>
    /// Per-session 64-bit random key. Sent back in <c>CS_LOGIN_ACK</c> as the trailing
    /// <c>dlCheckKey</c> field. Legacy uses it to XOR with <c>m_dlCheckFile</c> in the
    /// optional exec-file tamper check; we generate it but don't validate against it
    /// (the binary-integrity check is dead code in the legacy build).
    /// </summary>
    public long DlCheckKey { get; set; }

    /// <summary>
    /// Legacy <c>m_bAgreement</c>. Set to <c>true</c> on successful login when the
    /// account has already accepted the current EULA; set to <c>false</c> when login
    /// succeeded but the account hasn't accepted yet — in that case downstream lobby
    /// handlers reject every request except <c>CS_AGREEMENT_REQ</c>.
    /// </summary>
    public bool AgreementAccepted { get; set; }

    /// <summary>
    /// When <c>true</c>, <see cref="ClientConnection.DisposeAsync"/> SKIPS the
    /// TCURRENTUSER cleanup — the session is being handed off to MapSvr (post
    /// <c>CS_START_REQ</c>). Mirrors legacy <c>pUser->m_bLogout = FALSE</c>.
    /// </summary>
    public bool HandoffToMap { get; set; }

    /// <summary>World/group ID selected via CS_CHANNELLIST_REQ or CS_CHARLIST_REQ.</summary>
    public byte? SelectedGroupId { get; set; }

    /// <summary>Channel chosen at CS_START_REQ.</summary>
    public byte? SelectedChannel { get; set; }

    /// <summary>Character chosen at CS_START_REQ.</summary>
    public int? SelectedCharId { get; set; }

    /// <summary>
    /// Was this connection authenticated as a PCBang (Korean internet cafe) session?
    /// Populated by <see cref="LoginHandler"/> from <c>AuthSuccess.InPcBang</c>.
    /// Currently informational only — gameplay restrictions can read this flag.
    /// </summary>
    public bool InPcBang { get; set; }

    public bool IsAuthenticated => UserId is not null;
}
