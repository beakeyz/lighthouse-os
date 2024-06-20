const std = @import("../../std.zig");
const builtin = @import("../../lightos/builtin.zig");
const math = std.math;
const mem = std.mem;
const assert = std.assert;
const Allocator = mem.Allocator;

const Limb = std.math.big.Limb;
const DoubleLimb = std.math.big.DoubleLimb;
const Int = std.math.big.int.Managed;
const IntConst = std.math.big.int.Const;

/// An arbitrary-precision rational number.
///
/// Memory is allocated as needed for operations to ensure full precision is kept. The precision
/// of a Rational is only bounded by memory.
///
/// Rational's are always normalized. That is, for a Rational r = p/q where p and q are integers,
/// gcd(p, q) = 1 always.
///
/// TODO rework this to store its own allocator and use a non-managed big int, to avoid double
/// allocator storage.
pub const Rational = struct {
    /// Numerator. Determines the sign of the Rational.
    p: Int,

    /// Denominator. Sign is ignored.
    q: Int,

    /// Create a new Rational. A small amount of memory will be allocated on initialization.
    /// This will be 2 * Int.default_capacity.
    pub fn init(a: Allocator) !Rational {
        var p = try Int.init(a);
        errdefer p.deinit();
        return Rational{
            .p = p,
            .q = try Int.initSet(a, 1),
        };
    }

    /// Frees all memory associated with a Rational.
    pub fn deinit(self: *Rational) void {
        self.p.deinit();
        self.q.deinit();
    }

    /// Set a Rational from a primitive integer type.
    pub fn setInt(self: *Rational, a: anytype) !void {
        try self.p.set(a);
        try self.q.set(1);
    }

    /// Set a Rational from a string of the form `A/B` where A and B are base-10 integers.
    pub fn setFloatString(self: *Rational, str: []const u8) !void {
        // TODO: Accept a/b fractions and exponent form
        if (str.len == 0) {
            return error.InvalidFloatString;
        }

        const State = enum {
            Integer,
            Fractional,
        };

        var state = State.Integer;
        var point: ?usize = null;

        var start: usize = 0;
        if (str[0] == '-') {
            start += 1;
        }

        for (str, 0..) |c, i| {
            switch (state) {
                State.Integer => {
                    switch (c) {
                        '.' => {
                            state = State.Fractional;
                            point = i;
                        },
                        '0'...'9' => {
                            // okay
                        },
                        else => {
                            return error.InvalidFloatString;
                        },
                    }
                },
                State.Fractional => {
                    switch (c) {
                        '0'...'9' => {
                            // okay
                        },
                        else => {
                            return error.InvalidFloatString;
                        },
                    }
                },
            }
        }

        // TODO: batch the multiplies by 10
        if (point) |i| {
            try self.p.setString(10, str[0..i]);

            const base = IntConst{ .limbs = &[_]Limb{10}, .positive = true };
            var local_buf: [@sizeOf(Limb) * Int.default_capacity]u8 align(@alignOf(Limb)) = undefined;
            var fba = std.heap.FixedBufferAllocator.init(&local_buf);
            const base_managed = try base.toManaged(fba.allocator());

            var j: usize = start;
            while (j < str.len - i - 1) : (j += 1) {
                try self.p.ensureMulCapacity(self.p.toConst(), base);
                try self.p.mul(&self.p, &base_managed);
            }

            try self.q.setString(10, str[i + 1 ..]);
            try self.p.add(&self.p, &self.q);

            try self.q.set(1);
            var k: usize = i + 1;
            while (k < str.len) : (k += 1) {
                try self.q.mul(&self.q, &base_managed);
            }

            try self.reduce();
        } else {
            try self.p.setString(10, str[0..]);
            try self.q.set(1);
        }
    }

    /// Set a Rational from a floating-point value. The rational will have enough precision to
    /// completely represent the provided float.
    pub fn setFloat(self: *Rational, comptime T: type, f: T) !void {
        // Translated from golang.go/src/math/big/rat.go.
        assert(@typeInfo(T) == .Float);

        const UnsignedInt = std.meta.Int(.unsigned, @typeInfo(T).Float.bits);
        const f_bits = @as(UnsignedInt, @bitCast(f));

        const exponent_bits = math.floatExponentBits(T);
        const exponent_bias = (1 << (exponent_bits - 1)) - 1;
        const mantissa_bits = math.floatMantissaBits(T);

        const exponent_mask = (1 << exponent_bits) - 1;
        const mantissa_mask = (1 << mantissa_bits) - 1;

        var exponent = @as(i16, @intCast((f_bits >> mantissa_bits) & exponent_mask));
        var mantissa = f_bits & mantissa_mask;

        switch (exponent) {
            exponent_mask => {
                return error.NonFiniteFloat;
            },
            0 => {
                // denormal
                exponent -= exponent_bias - 1;
            },
            else => {
                // normal
                mantissa |= 1 << mantissa_bits;
                exponent -= exponent_bias;
            },
        }

        var shift: i16 = mantissa_bits - exponent;

        // factor out powers of two early from rational
        while (mantissa & 1 == 0 and shift > 0) {
            mantissa >>= 1;
            shift -= 1;
        }

        try self.p.set(mantissa);
        self.p.setSign(f >= 0);

        try self.q.set(1);
        if (shift >= 0) {
            try self.q.shiftLeft(&self.q, @as(usize, @intCast(shift)));
        } else {
            try self.p.shiftLeft(&self.p, @as(usize, @intCast(-shift)));
        }

        try self.reduce();
    }

    /// Return a floating-point value that is the closest value to a Rational.
    ///
    /// The result may not be exact if the Rational is too precise or too large for the
    /// target type.
    pub fn toFloat(self: Rational, comptime T: type) !T {
        // Translated from golang.go/src/math/big/rat.go.
        // TODO: Indicate whether the result is not exact.
        assert(@typeInfo(T) == .Float);

        const fsize = @typeInfo(T).Float.bits;
        const BitReprType = std.meta.Int(.unsigned, fsize);

        const msize = math.floatMantissaBits(T);
        const msize1 = msize + 1;
        const msize2 = msize1 + 1;

        const esize = math.floatExponentBits(T);
        const ebias = (1 << (esize - 1)) - 1;
        const emin = 1 - ebias;

        if (self.p.eqlZero()) {
            return 0;
        }

        // 1. left-shift a or sub so that a/b is in [1 << msize1, 1 << (msize2 + 1)]
        var exp = @as(isize, @intCast(self.p.bitCountTwosComp())) - @as(isize, @intCast(self.q.bitCountTwosComp()));

        var a2 = try self.p.clone();
        defer a2.deinit();

        var b2 = try self.q.clone();
        defer b2.deinit();

        const shift = msize2 - exp;
        if (shift >= 0) {
            try a2.shiftLeft(&a2, @as(usize, @intCast(shift)));
        } else {
            try b2.shiftLeft(&b2, @as(usize, @intCast(-shift)));
        }

        // 2. compute quotient and remainder
        var q = try Int.init(self.p.allocator);
        defer q.deinit();

        // unused
        var r = try Int.init(self.p.allocator);
        defer r.deinit();

        try Int.divTrunc(&q, &r, &a2, &b2);

        var mantissa = extractLowBits(q, BitReprType);
        var have_rem = r.len() > 0;

        // 3. q didn't fit in msize2 bits, redo division b2 << 1
        if (mantissa >> msize2 == 1) {
            if (mantissa & 1 == 1) {
                have_rem = true;
            }
            mantissa >>= 1;
            exp += 1;
        }
        if (mantissa >> msize1 != 1) {
            // NOTE: This can be hit if the limb size is small (u8/16).
            @panic("unexpected bits in result");
        }

        // 4. Rounding
        if (emin - msize <= exp and exp <= emin) {
            // denormal
            const shift1 = @as(math.Log2Int(BitReprType), @intCast(emin - (exp - 1)));
            const lost_bits = mantissa & ((@as(BitReprType, @intCast(1)) << shift1) - 1);
            have_rem = have_rem or lost_bits != 0;
            mantissa >>= shift1;
            exp = 2 - ebias;
        }

        // round q using round-half-to-even
        var exact = !have_rem;
        if (mantissa & 1 != 0) {
            exact = false;
            if (have_rem or (mantissa & 2 != 0)) {
                mantissa += 1;
                if (mantissa >= 1 << msize2) {
                    // 11...1 => 100...0
                    mantissa >>= 1;
                    exp += 1;
                }
            }
        }
        mantissa >>= 1;

        const f = math.scalbn(@as(T, @floatFromInt(mantissa)), @as(i32, @intCast(exp - msize1)));
        if (math.isInf(f)) {
            exact = false;
        }

        return if (self.p.isPositive()) f else -f;
    }

    /// Set a rational from an integer ratio.
    pub fn setRatio(self: *Rational, p: anytype, q: anytype) !void {
        try self.p.set(p);
        try self.q.set(q);

        self.p.setSign(@intFromBool(self.p.isPositive()) ^ @intFromBool(self.q.isPositive()) == 0);
        self.q.setSign(true);

        try self.reduce();

        if (self.q.eqlZero()) {
            @panic("cannot set rational with denominator = 0");
        }
    }

    /// Set a Rational directly from an Int.
    pub fn copyInt(self: *Rational, a: Int) !void {
        try self.p.copy(a.toConst());
        try self.q.set(1);
    }

    /// Set a Rational directly from a ratio of two Int's.
    pub fn copyRatio(self: *Rational, a: Int, b: Int) !void {
        try self.p.copy(a.toConst());
        try self.q.copy(b.toConst());

        self.p.setSign(@intFromBool(self.p.isPositive()) ^ @intFromBool(self.q.isPositive()) == 0);
        self.q.setSign(true);

        try self.reduce();
    }

    /// Make a Rational positive.
    pub fn abs(r: *Rational) void {
        r.p.abs();
    }

    /// Negate the sign of a Rational.
    pub fn negate(r: *Rational) void {
        r.p.negate();
    }

    /// Efficiently swap a Rational with another. This swaps the limb pointers and a full copy is not
    /// performed. The address of the limbs field will not be the same after this function.
    pub fn swap(r: *Rational, other: *Rational) void {
        r.p.swap(&other.p);
        r.q.swap(&other.q);
    }

    /// Returns math.Order.lt, math.Order.eq, math.Order.gt if a < b, a == b or
    /// a > b respectively.
    pub fn order(a: Rational, b: Rational) !math.Order {
        return cmpInternal(a, b, false);
    }

    /// Returns math.Order.lt, math.Order.eq, math.Order.gt if |a| < |b|, |a| ==
    /// |b| or |a| > |b| respectively.
    pub fn orderAbs(a: Rational, b: Rational) !math.Order {
        return cmpInternal(a, b, true);
    }

    // p/q > x/y iff p*y > x*q
    fn cmpInternal(a: Rational, b: Rational, is_abs: bool) !math.Order {
        // TODO: Would a div compare algorithm of sorts be viable and quicker? Can we avoid
        // the memory allocations here?
        var q = try Int.init(a.p.allocator);
        defer q.deinit();

        var p = try Int.init(b.p.allocator);
        defer p.deinit();

        try q.mul(&a.p, &b.q);
        try p.mul(&b.p, &a.q);

        return if (is_abs) q.orderAbs(p) else q.order(p);
    }

    /// rma = a + b.
    ///
    /// rma, a and b may be aliases. However, it is more efficient if rma does not alias a or b.
    ///
    /// Returns an error if memory could not be allocated.
    pub fn add(rma: *Rational, a: Rational, b: Rational) !void {
        var r = rma;
        var aliased = rma.p.limbs.ptr == a.p.limbs.ptr or rma.p.limbs.ptr == b.p.limbs.ptr;

        var sr: Rational = undefined;
        if (aliased) {
            sr = try Rational.init(rma.p.allocator);
            r = &sr;
            aliased = true;
        }
        defer if (aliased) {
            rma.swap(r);
            r.deinit();
        };

        try r.p.mul(&a.p, &b.q);
        try r.q.mul(&b.p, &a.q);
        try r.p.add(&r.p, &r.q);

        try r.q.mul(&a.q, &b.q);
        try r.reduce();
    }

    /// rma = a - b.
    ///
    /// rma, a and b may be aliases. However, it is more efficient if rma does not alias a or b.
    ///
    /// Returns an error if memory could not be allocated.
    pub fn sub(rma: *Rational, a: Rational, b: Rational) !void {
        var r = rma;
        var aliased = rma.p.limbs.ptr == a.p.limbs.ptr or rma.p.limbs.ptr == b.p.limbs.ptr;

        var sr: Rational = undefined;
        if (aliased) {
            sr = try Rational.init(rma.p.allocator);
            r = &sr;
            aliased = true;
        }
        defer if (aliased) {
            rma.swap(r);
            r.deinit();
        };

        try r.p.mul(&a.p, &b.q);
        try r.q.mul(&b.p, &a.q);
        try r.p.sub(&r.p, &r.q);

        try r.q.mul(&a.q, &b.q);
        try r.reduce();
    }

    /// rma = a * b.
    ///
    /// rma, a and b may be aliases. However, it is more efficient if rma does not alias a or b.
    ///
    /// Returns an error if memory could not be allocated.
    pub fn mul(r: *Rational, a: Rational, b: Rational) !void {
        try r.p.mul(&a.p, &b.p);
        try r.q.mul(&a.q, &b.q);
        try r.reduce();
    }

    /// rma = a / b.
    ///
    /// rma, a and b may be aliases. However, it is more efficient if rma does not alias a or b.
    ///
    /// Returns an error if memory could not be allocated.
    pub fn div(r: *Rational, a: Rational, b: Rational) !void {
        if (b.p.eqlZero()) {
            @panic("division by zero");
        }

        try r.p.mul(&a.p, &b.q);
        try r.q.mul(&b.p, &a.q);
        try r.reduce();
    }

    /// Invert the numerator and denominator fields of a Rational. p/q => q/p.
    pub fn invert(r: *Rational) void {
        Int.swap(&r.p, &r.q);
    }

    // reduce r/q such that gcd(r, q) = 1
    fn reduce(r: *Rational) !void {
        var a = try Int.init(r.p.allocator);
        defer a.deinit();

        const sign = r.p.isPositive();
        r.p.abs();
        try a.gcd(&r.p, &r.q);
        r.p.setSign(sign);

        const one = IntConst{ .limbs = &[_]Limb{1}, .positive = true };
        if (a.toConst().order(one) != .eq) {
            var unused = try Int.init(r.p.allocator);
            defer unused.deinit();

            // TODO: divexact would be useful here
            // TODO: don't copy r.q for div
            try Int.divTrunc(&r.p, &unused, &r.p, &a);
            try Int.divTrunc(&r.q, &unused, &r.q, &a);
        }
    }
};

fn extractLowBits(a: Int, comptime T: type) T {
    assert(@typeInfo(T) == .Int);

    const t_bits = @typeInfo(T).Int.bits;
    const limb_bits = @typeInfo(Limb).Int.bits;
    if (t_bits <= limb_bits) {
        return @as(T, @truncate(a.limbs[0]));
    } else {
        var r: T = 0;
        comptime var i: usize = 0;

        // Remainder is always 0 since if t_bits >= limb_bits -> Limb | T and both
        // are powers of two.
        inline while (i < t_bits / limb_bits) : (i += 1) {
            r |= math.shl(T, a.limbs[i], i * limb_bits);
        }

        return r;
    }
}
