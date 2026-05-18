using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class dtproperty
{
    public int id { get; set; }

    public int? objectid { get; set; }

    public string property { get; set; } = null!;

    public string? value { get; set; }

    public string? uvalue { get; set; }

    public byte[]? lvalue { get; set; }

    public int version { get; set; }
}
