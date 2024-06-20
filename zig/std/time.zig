const std = @import("std.zig");
const builtin = @import("lightos/builtin.zig");
const assert = std.debug.assert;
const math = std.math;

pub const epoch = @import("time/epoch.zig");

/// Spurious wakeups are possible and no precision of timing is guaranteed.
pub fn sleep(nanoseconds: u64) void {
    _ = nanoseconds;
    std.lightos_abi.kernel_panic("TODO: time.zig: sleep");
}

/// Get a calendar timestamp, in seconds, relative to UTC 1970-01-01.
/// Precision of timing depends on the hardware and operating system.
/// The return value is signed because it is possible to have a date that is
/// before the epoch.
/// See `std.os.clock_gettime` for a POSIX timestamp.
pub fn timestamp() i64 {
    return @divFloor(milliTimestamp(), ms_per_s);
}

/// Get a calendar timestamp, in milliseconds, relative to UTC 1970-01-01.
/// Precision of timing depends on the hardware and operating system.
/// The return value is signed because it is possible to have a date that is
/// before the epoch.
/// See `std.os.clock_gettime` for a POSIX timestamp.
pub fn milliTimestamp() i64 {
    return @as(i64, @intCast(@divFloor(nanoTimestamp(), ns_per_ms)));
}

/// Get a calendar timestamp, in microseconds, relative to UTC 1970-01-01.
/// Precision of timing depends on the hardware and operating system.
/// The return value is signed because it is possible to have a date that is
/// before the epoch.
/// See `std.os.clock_gettime` for a POSIX timestamp.
pub fn microTimestamp() i64 {
    return @as(i64, @intCast(@divFloor(nanoTimestamp(), ns_per_us)));
}

/// Get a calendar timestamp, in nanoseconds, relative to UTC 1970-01-01.
/// Precision of timing depends on the hardware and operating system.
/// On Windows this has a maximum granularity of 100 nanoseconds.
/// The return value is signed because it is possible to have a date that is
/// before the epoch.
/// See `std.os.clock_gettime` for a POSIX timestamp.
pub fn nanoTimestamp() i128 {
    std.lightos_abi.kernel_panic("TODO: time.zig: sleep");
}

// Divisions of a nanosecond.
pub const ns_per_us = 1000;
pub const ns_per_ms = 1000 * ns_per_us;
pub const ns_per_s = 1000 * ns_per_ms;
pub const ns_per_min = 60 * ns_per_s;
pub const ns_per_hour = 60 * ns_per_min;
pub const ns_per_day = 24 * ns_per_hour;
pub const ns_per_week = 7 * ns_per_day;

// Divisions of a microsecond.
pub const us_per_ms = 1000;
pub const us_per_s = 1000 * us_per_ms;
pub const us_per_min = 60 * us_per_s;
pub const us_per_hour = 60 * us_per_min;
pub const us_per_day = 24 * us_per_hour;
pub const us_per_week = 7 * us_per_day;

// Divisions of a millisecond.
pub const ms_per_s = 1000;
pub const ms_per_min = 60 * ms_per_s;
pub const ms_per_hour = 60 * ms_per_min;
pub const ms_per_day = 24 * ms_per_hour;
pub const ms_per_week = 7 * ms_per_day;

// Divisions of a second.
pub const s_per_min = 60;
pub const s_per_hour = s_per_min * 60;
pub const s_per_day = s_per_hour * 24;
pub const s_per_week = s_per_day * 7;

/// An Instant represents a timestamp with respect to the currently
/// executing program that ticks during suspend and can be used to
/// record elapsed time unlike `nanoTimestamp`.
///
/// It tries to sample the system's fastest and most precise timer available.
/// It also tries to be monotonic, but this is not a guarantee due to OS/hardware bugs.
/// If you need monotonic readings for elapsed time, consider `Timer` instead.
pub const Instant = struct {
    timestamp: u64,

    // true if we should use clock_gettime()
    const is_posix = switch (builtin.os.tag) {
        .windows, .uefi, .wasi => false,
        else => true,
    };

    /// Queries the system for the current moment of time as an Instant.
    /// This is not guaranteed to be monotonic or steadily increasing, but for
    /// most implementations it is.
    /// Returns `error.Unsupported` when a suitable clock is not detected.
    pub fn now() error{Unsupported}!Instant {
        return error.Unsupported;
    }

    /// Quickly compares two instances between each other.
    pub fn order(self: Instant, other: Instant) std.math.Order {
        // windows and wasi timestamps are in u64 which is easily comparible
        if (!is_posix) {
            return std.math.order(self.timestamp, other.timestamp);
        }

        var ord = std.math.order(self.timestamp.tv_sec, other.timestamp.tv_sec);
        if (ord == .eq) {
            ord = std.math.order(self.timestamp.tv_nsec, other.timestamp.tv_nsec);
        }
        return ord;
    }

    /// Returns elapsed time in nanoseconds since the `earlier` Instant.
    /// This assumes that the `earlier` Instant represents a moment in time before or equal to `self`.
    /// This also assumes that the time that has passed between both Instants fits inside a u64 (~585 yrs).
    pub fn since(self: Instant, earlier: Instant) u64 {
        // Convert timespec diff to ns
        const seconds = @as(u64, @intCast(self.timestamp.tv_sec - earlier.timestamp.tv_sec));
        const elapsed = (seconds * ns_per_s) + @as(u32, @intCast(self.timestamp.tv_nsec));
        return elapsed - @as(u32, @intCast(earlier.timestamp.tv_nsec));
    }
};

/// A monotonic, high performance timer.
///
/// Timer.start() is used to initialize the timer
/// and gives the caller an opportunity to check for the existence of a supported clock.
/// Once a supported clock is discovered,
/// it is assumed that it will be available for the duration of the Timer's use.
///
/// Monotonicity is ensured by saturating on the most previous sample.
/// This means that while timings reported are monotonic,
/// they're not guaranteed to tick at a steady rate as this is up to the underlying system.
pub const Timer = struct {
    started: Instant,
    previous: Instant,

    pub const Error = error{TimerUnsupported};

    /// Initialize the timer by querying for a supported clock.
    /// Returns `error.TimerUnsupported` when such a clock is unavailable.
    /// This should only fail in hostile environments such as linux seccomp misuse.
    pub fn start() Error!Timer {
        const current = Instant.now() catch return error.TimerUnsupported;
        return Timer{ .started = current, .previous = current };
    }

    /// Reads the timer value since start or the last reset in nanoseconds.
    pub fn read(self: *Timer) u64 {
        const current = self.sample();
        return current.since(self.started);
    }

    /// Resets the timer value to 0/now.
    pub fn reset(self: *Timer) void {
        const current = self.sample();
        self.started = current;
    }

    /// Returns the current value of the timer in nanoseconds, then resets it.
    pub fn lap(self: *Timer) u64 {
        const current = self.sample();
        defer self.started = current;
        return current.since(self.started);
    }

    /// Returns an Instant sampled at the callsite that is
    /// guaranteed to be monotonic with respect to the timer's starting point.
    fn sample(self: *Timer) Instant {
        const current = Instant.now() catch unreachable;
        if (current.order(self.previous) == .gt) {
            self.previous = current;
        }
        return self.previous;
    }
};
