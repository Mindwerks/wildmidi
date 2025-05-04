const std = @import("std");
const wm = @import("wildmidi");
const testing = std.testing;

test {
    try wm.init("wildmidi.cfg", 44100, .{});
}
