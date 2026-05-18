using System.Collections.Concurrent;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Auth;

/// <summary>
/// Tracks failed login attempts per IP and per username over a sliding window so
/// the server can shed brute-force / credential-stuffing traffic without locking
/// out legitimate users on transient typos.
///
/// Behaviour:
///   • On a failed login, call <see cref="RegisterFailure"/>.
///   • Before authentication, call <see cref="IsBlocked"/> — if either the IP
///     or the username has exceeded the failure threshold in the window, the
///     login is rejected with <see cref="FourStory.Shared.LoginResult.RateLimited"/>.
///   • On success, call <see cref="RegisterSuccess"/> to clear the user's bucket
///     (so a returning user who typo'd once doesn't burn through the budget).
///
/// Implementation: bounded queue of timestamps per key. Counts trim themselves
/// on access (no background reaper needed because untouched buckets are GC'd via
/// a periodic prune pass). Thread-safe via per-bucket lock.
/// </summary>
public sealed class LoginRateLimiter
{
    /// <summary>Failures per IP allowed in the window (legit users typically only fail 1-2 times).</summary>
    public const int IpThreshold = 20;
    /// <summary>Failures per username allowed in the window.</summary>
    public const int UserThreshold = 5;
    public static readonly TimeSpan Window = TimeSpan.FromMinutes(1);

    private readonly ConcurrentDictionary<string, Bucket> _ipBuckets = new(StringComparer.OrdinalIgnoreCase);
    private readonly ConcurrentDictionary<string, Bucket> _userBuckets = new(StringComparer.OrdinalIgnoreCase);
    private readonly ILogger<LoginRateLimiter> _logger;
    private readonly TimeProvider _clock;
    private long _lastPruneTicks;

    public LoginRateLimiter(ILogger<LoginRateLimiter> logger, TimeProvider? clock = null)
    {
        _logger = logger;
        _clock = clock ?? TimeProvider.System;
        _lastPruneTicks = _clock.GetUtcNow().UtcTicks;
    }

    public bool IsBlocked(string ip, string userId)
    {
        MaybePrune();
        var now = _clock.GetUtcNow();

        if (_ipBuckets.TryGetValue(ip, out var ipBucket) && ipBucket.CountAfter(now - Window) >= IpThreshold)
        {
            _logger.LogWarning("Login throttled (IP burst): ip={Ip} threshold={Threshold}", ip, IpThreshold);
            return true;
        }
        if (_userBuckets.TryGetValue(userId, out var userBucket) && userBucket.CountAfter(now - Window) >= UserThreshold)
        {
            _logger.LogWarning("Login throttled (user burst): user={UserId} threshold={Threshold}", userId, UserThreshold);
            return true;
        }
        return false;
    }

    public void RegisterFailure(string ip, string userId)
    {
        var now = _clock.GetUtcNow();
        _ipBuckets.GetOrAdd(ip, _ => new Bucket()).Add(now);
        _userBuckets.GetOrAdd(userId, _ => new Bucket()).Add(now);
    }

    /// <summary>Clear this user's failure bucket so successful logins reset the count.</summary>
    public void RegisterSuccess(string userId)
    {
        if (_userBuckets.TryGetValue(userId, out var bucket))
        {
            bucket.Clear();
        }
    }

    private void MaybePrune()
    {
        // Prune at most once per Window to keep the cost amortized.
        var nowTicks = _clock.GetUtcNow().UtcTicks;
        var lastTicks = Interlocked.Read(ref _lastPruneTicks);
        if (nowTicks - lastTicks < Window.Ticks)
        {
            return;
        }
        if (Interlocked.CompareExchange(ref _lastPruneTicks, nowTicks, lastTicks) != lastTicks)
        {
            return;
        }
        var cutoff = _clock.GetUtcNow() - Window;
        PruneMap(_ipBuckets, cutoff);
        PruneMap(_userBuckets, cutoff);
    }

    private static void PruneMap(ConcurrentDictionary<string, Bucket> map, DateTimeOffset cutoff)
    {
        foreach (var kvp in map)
        {
            if (kvp.Value.CountAfter(cutoff) == 0)
            {
                map.TryRemove(kvp.Key, out _);
            }
        }
    }

    private sealed class Bucket
    {
        private readonly Queue<DateTimeOffset> _stamps = new();
        private readonly object _gate = new();

        public void Add(DateTimeOffset now)
        {
            lock (_gate)
            {
                _stamps.Enqueue(now);
                // Cap memory: never keep more than 2× the most aggressive threshold worth.
                while (_stamps.Count > IpThreshold * 2)
                {
                    _stamps.Dequeue();
                }
            }
        }

        public int CountAfter(DateTimeOffset cutoff)
        {
            lock (_gate)
            {
                // Cheap pre-trim while we hold the lock.
                while (_stamps.Count > 0 && _stamps.Peek() < cutoff)
                {
                    _stamps.Dequeue();
                }
                return _stamps.Count;
            }
        }

        public void Clear()
        {
            lock (_gate) { _stamps.Clear(); }
        }
    }
}
