namespace FourStory.Protocol;

public static class ProtocolConstants
{
    public const ushort Version = 0x2918;

    public const int DefaultPacketSize = 1024;
    public const int MaxPacketSize = 0xFFFF;

    /// <summary>Legacy <c>MAX_NAME</c> — caps userId, password, char name length.</summary>
    public const int MaxNameLength = 16;

    /// <summary>Magic constant the client embeds in <c>CS_TERMINATE_REQ</c> (<c>CSHandler.cpp:1452</c>).</summary>
    public const uint TerminateMagic = 720809425; // 0x2AF3A9D1

    public const ushort SmBase = 0x1581;
    public const ushort MwBase = 0x9001;
    public const ushort DmBase = 0x5891;
    public const ushort CsLogin = 0x1987;
    public const ushort CsMap = 0x5280;
    public const ushort CtControl = 0x9301;
    public const ushort CtPatch = 0x4201;
    public const ushort RwRelay = 0x9999;
    public const ushort CsCustom = 0x3312;

    public const ushort DefaultControlPort = 3615;
    public const ushort DefaultPatchPort = 3715;
    public const ushort DefaultWorldPort = 3815;
    public const ushort DefaultLoginPort = 4815;
    public const ushort DefaultMapPort = 5815;
}
