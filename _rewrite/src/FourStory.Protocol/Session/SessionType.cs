namespace FourStory.Protocol.Session;

/// <summary>
/// What kind of peer is on the other end of a session. Drives crypto behavior
/// (clients get the outer RC4 layer, servers don't).
/// </summary>
public enum PeerType
{
    /// <summary>Peer is another server (inter-server traffic, plaintext on the wire).</summary>
    Server = 0,

    /// <summary>Peer is a game client (RC4 + XOR layered encryption).</summary>
    Client = 1,
}
