const std = @import("std.zig");
const mem = std.mem;
const math = std.math;
const assert = std.assert;

pub const Mode = enum { stable, unstable };

//pub const block = @import("sort/block.zig").block;
//pub const pdq = @import("sort/pdq.zig").pdq;
//pub const pdqContext = @import("sort/pdq.zig").pdqContext;

/// Stable in-place sort. O(n) best case, O(pow(n, 2)) worst case.
/// O(1) memory (no allocator required).
/// Sorts in ascending order with respect to the given `lessThan` function.
pub fn insertion(
    comptime T: type,
    items: []T,
    context: anytype,
    comptime lessThanFn: fn (@TypeOf(context), lhs: T, rhs: T) bool,
) void {
    const Context = struct {
        items: []T,
        sub_ctx: @TypeOf(context),

        pub fn lessThan(ctx: @This(), a: usize, b: usize) bool {
            return lessThanFn(ctx.sub_ctx, ctx.items[a], ctx.items[b]);
        }

        pub fn swap(ctx: @This(), a: usize, b: usize) void {
            return mem.swap(T, &ctx.items[a], &ctx.items[b]);
        }
    };
    insertionContext(0, items.len, Context{ .items = items, .sub_ctx = context });
}

/// Stable in-place sort. O(n) best case, O(pow(n, 2)) worst case.
/// O(1) memory (no allocator required).
/// `context` must have methods `swap` and `lessThan`,
/// which each take 2 `usize` parameters indicating the index of an item.
/// Sorts in ascending order with respect to `lessThan`.
pub fn insertionContext(a: usize, b: usize, context: anytype) void {
    assert(a <= b);

    var i = a + 1;
    while (i < b) : (i += 1) {
        var j = i;
        while (j > a and context.lessThan(j, j - 1)) : (j -= 1) {
            context.swap(j, j - 1);
        }
    }
}

/// Unstable in-place sort. O(n*log(n)) best case, worst case and average case.
/// O(1) memory (no allocator required).
/// Sorts in ascending order with respect to the given `lessThan` function.
pub fn heap(
    comptime T: type,
    items: []T,
    context: anytype,
    comptime lessThanFn: fn (@TypeOf(context), lhs: T, rhs: T) bool,
) void {
    const Context = struct {
        items: []T,
        sub_ctx: @TypeOf(context),

        pub fn lessThan(ctx: @This(), a: usize, b: usize) bool {
            return lessThanFn(ctx.sub_ctx, ctx.items[a], ctx.items[b]);
        }

        pub fn swap(ctx: @This(), a: usize, b: usize) void {
            return mem.swap(T, &ctx.items[a], &ctx.items[b]);
        }
    };
    heapContext(0, items.len, Context{ .items = items, .sub_ctx = context });
}

/// Unstable in-place sort. O(n*log(n)) best case, worst case and average case.
/// O(1) memory (no allocator required).
/// `context` must have methods `swap` and `lessThan`,
/// which each take 2 `usize` parameters indicating the index of an item.
/// Sorts in ascending order with respect to `lessThan`.
pub fn heapContext(a: usize, b: usize, context: anytype) void {
    assert(a <= b);
    // build the heap in linear time.
    var i = a + (b - a) / 2;
    while (i > a) {
        i -= 1;
        siftDown(a, i, b, context);
    }

    // pop maximal elements from the heap.
    i = b;
    while (i > a) {
        i -= 1;
        context.swap(a, i);
        siftDown(a, a, i, context);
    }
}

fn siftDown(a: usize, target: usize, b: usize, context: anytype) void {
    var cur = target;
    while (true) {
        // When we don't overflow from the multiply below, the following expression equals (2*cur) - (2*a) + a + 1
        // The `+ a + 1` is safe because:
        //  for `a > 0` then `2a >= a + 1`.
        //  for `a = 0`, the expression equals `2*cur+1`. `2*cur` is an even number, therefore adding 1 is safe.
        var child = (math.mul(usize, cur - a, 2) catch break) + a + 1;

        // stop if we overshot the boundary
        if (!(child < b)) break;

        // `next_child` is at most `b`, therefore no overflow is possible
        const next_child = child + 1;

        // store the greater child in `child`
        if (next_child < b and context.lessThan(child, next_child)) {
            child = next_child;
        }

        // stop if the Heap invariant holds at `cur`.
        if (context.lessThan(child, cur)) break;

        // swap `cur` with the greater child,
        // move one step down, and continue sifting.
        context.swap(child, cur);
        cur = child;
    }
}

/// Use to generate a comparator function for a given type. e.g. `sort(u8, slice, {}, asc(u8))`.
pub fn asc(comptime T: type) fn (void, T, T) bool {
    return struct {
        pub fn inner(_: void, a: T, b: T) bool {
            return a < b;
        }
    }.inner;
}

/// Use to generate a comparator function for a given type. e.g. `sort(u8, slice, {}, desc(u8))`.
pub fn desc(comptime T: type) fn (void, T, T) bool {
    return struct {
        pub fn inner(_: void, a: T, b: T) bool {
            return a > b;
        }
    }.inner;
}

const asc_u8 = asc(u8);
const asc_i32 = asc(i32);
const desc_u8 = desc(u8);
const desc_i32 = desc(i32);

const sort_funcs = &[_]fn (comptime type, anytype, anytype, comptime anytype) void{
    //block,
    //pdq,
    insertion,
    heap,
};

const context_sort_funcs = &[_]fn (usize, usize, anytype) void{
    // blockContext,
    //pdqContext,
    insertionContext,
    heapContext,
};

const IdAndValue = struct {
    id: usize,
    value: i32,

    fn lessThan(context: void, a: IdAndValue, b: IdAndValue) bool {
        _ = context;
        return a.value < b.value;
    }
};

/// Returns the index of an element in `items` equal to `key`.
/// If there are multiple such elements, returns the index of any one of them.
/// If there are no such elements, returns `null`.
///
/// `items` must be sorted in ascending order with respect to `compareFn`.
///
/// O(log n) complexity.
pub fn binarySearch(
    comptime T: type,
    key: anytype,
    items: []const T,
    context: anytype,
    comptime compareFn: fn (context: @TypeOf(context), key: @TypeOf(key), mid_item: T) math.Order,
) ?usize {
    var left: usize = 0;
    var right: usize = items.len;

    while (left < right) {
        // Avoid overflowing in the midpoint calculation
        const mid = left + (right - left) / 2;
        // Compare the key with the midpoint element
        switch (compareFn(context, key, items[mid])) {
            .eq => return mid,
            .gt => left = mid + 1,
            .lt => right = mid,
        }
    }

    return null;
}

/// Returns the index of the first element in `items` greater than or equal to `key`,
/// or `items.len` if all elements are less than `key`.
///
/// `items` must be sorted in ascending order with respect to `compareFn`.
///
/// O(log n) complexity.
pub fn lowerBound(
    comptime T: type,
    key: anytype,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (context: @TypeOf(context), lhs: @TypeOf(key), rhs: T) bool,
) usize {
    var left: usize = 0;
    var right: usize = items.len;

    while (left < right) {
        const mid = left + (right - left) / 2;
        if (lessThan(context, items[mid], key)) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return left;
}

/// Returns the index of the first element in `items` greater than `key`,
/// or `items.len` if all elements are less than or equal to `key`.
///
/// `items` must be sorted in ascending order with respect to `compareFn`.
///
/// O(log n) complexity.
pub fn upperBound(
    comptime T: type,
    key: anytype,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (context: @TypeOf(context), lhs: @TypeOf(key), rhs: T) bool,
) usize {
    var left: usize = 0;
    var right: usize = items.len;

    while (left < right) {
        const mid = (right + left) / 2;
        if (!lessThan(context, key, items[mid])) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return left;
}

/// Returns a tuple of the lower and upper indices in `items` between which all elements are equal to `key`.
/// If no element in `items` is equal to `key`, both indices are the
/// index of the first element in `items` greater than `key`.
/// If no element in `items` is greater than `key`, both indices equal `items.len`.
///
/// `items` must be sorted in ascending order with respect to `compareFn`.
///
/// O(log n) complexity.
///
/// See also: `lowerBound` and `upperBound`.
pub fn equalRange(
    comptime T: type,
    key: anytype,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (context: @TypeOf(context), lhs: @TypeOf(key), rhs: T) bool,
) struct { usize, usize } {
    return .{
        lowerBound(T, key, items, context, lessThan),
        upperBound(T, key, items, context, lessThan),
    };
}

pub fn argMin(
    comptime T: type,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (@TypeOf(context), lhs: T, rhs: T) bool,
) ?usize {
    if (items.len == 0) {
        return null;
    }

    var smallest = items[0];
    var smallest_index: usize = 0;
    for (items[1..], 0..) |item, i| {
        if (lessThan(context, item, smallest)) {
            smallest = item;
            smallest_index = i + 1;
        }
    }

    return smallest_index;
}

pub fn min(
    comptime T: type,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (context: @TypeOf(context), lhs: T, rhs: T) bool,
) ?T {
    const i = argMin(T, items, context, lessThan) orelse return null;
    return items[i];
}

pub fn argMax(
    comptime T: type,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (context: @TypeOf(context), lhs: T, rhs: T) bool,
) ?usize {
    if (items.len == 0) {
        return null;
    }

    var biggest = items[0];
    var biggest_index: usize = 0;
    for (items[1..], 0..) |item, i| {
        if (lessThan(context, biggest, item)) {
            biggest = item;
            biggest_index = i + 1;
        }
    }

    return biggest_index;
}

pub fn max(
    comptime T: type,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (context: @TypeOf(context), lhs: T, rhs: T) bool,
) ?T {
    const i = argMax(T, items, context, lessThan) orelse return null;
    return items[i];
}

pub fn isSorted(
    comptime T: type,
    items: []const T,
    context: anytype,
    comptime lessThan: fn (context: @TypeOf(context), lhs: T, rhs: T) bool,
) bool {
    var i: usize = 1;
    while (i < items.len) : (i += 1) {
        if (lessThan(context, items[i], items[i - 1])) {
            return false;
        }
    }

    return true;
}
