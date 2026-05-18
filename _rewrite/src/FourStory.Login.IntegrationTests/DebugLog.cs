using System.Collections.Concurrent;

namespace FourStory.Login.IntegrationTests;

/// <summary>
/// Captures debug messages from MockClient that xUnit's stdout-eating prevents us from
/// seeing otherwise. Cleared at the start of each test via the fixture.
/// </summary>
internal static class DebugLog
{
    private static readonly ConcurrentQueue<string> _entries = new();

    public static void Add(string msg) => _entries.Enqueue($"[{DateTime.UtcNow:HH:mm:ss.fff}] {msg}");

    public static string Dump()
    {
        var sb = new System.Text.StringBuilder();
        foreach (var e in _entries)
        {
            sb.AppendLine(e);
        }
        return sb.ToString();
    }

    public static void Clear() => _entries.Clear();
}
