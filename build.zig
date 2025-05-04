const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "wildmidi",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();
    lib.addIncludePath(b.path("include"));

    const source_files = .{
        "src/wm_error.c",
        "src/file_io.c",
        "src/lock.c",
        "src/wildmidi_lib.c",
        "src/reverb.c",
        "src/gus_pat.c",
        "src/internal_midi.c",
        "src/patches.c",
        "src/f_xmidi.c",
        "src/f_mus.c",
        "src/f_hmp.c",
        "src/f_hmi.c",
        "src/f_midi.c",
        "src/sample.c",
        "src/mus2mid.c",
        "src/xmi2mid.c",
    };

    const defaultFlags = .{
        "-Wall",
        "-W",
        "-fno-common",
        "-DWILDMIDI_BUILD",
        "-g",
    };

    const config_header = b.addConfigHeader(
        .{
            .style = .{
                .cmake = b.path("include/config.h.cmake"),
            },
        },
        .{
            .WILDMIDI_CFG = b.pathFromRoot("cfg/wildmidi.cfg"),
            .WILDMIDI_VERSION = getVersionFromZon(),
            .HAVE_C_INLINE = 1,
            .HAVE_C___INLINE = 1,
            .HAVE_C___INLINE__ = 1,
            .HAVE___BUILTIN_EXPECT = 1,
            .HAVE_STDINT_H = 1,
            .HAVE_INTTYPES_H = 1,
            .WORDS_BIGENDIAN = null,
            .WILDMIDI_AMIGA = null,
            .HAVE_SYS_SOUNDCARD_H = null,
            .AUDIODRV_ALSA = null,
            .AUDIODRV_OSS = null,
            .AUDIODRV_AHI = null,
            .AUDIODRV_OPENAL = null,
        },
    );

    lib.addConfigHeader(config_header);

    switch (target.result.os.tag) {
        .windows => {
            lib.addCSourceFiles(.{
                .files = &source_files,
                .flags = &(.{"-DWILDMIDI_STATIC"} ++ defaultFlags),
            });
            lib.addIncludePath(b.path("mingw"));
        },
        .macos => {
            lib.addCSourceFiles(.{
                .files = &source_files,
                .flags = &defaultFlags,
            });
            lib.addIncludePath(b.path("macosx"));
        },
        else => {
            lib.addCSourceFiles(.{
                .files = &source_files,
                .flags = &defaultFlags,
            });
        },
    }

    b.installArtifact(lib);

    _ = b.addModule("wildmidi", .{ .root_source_file = b.path("src/root.zig") });
}

fn getVersionFromZon() []const u8 {
    const build_zig_zon = @embedFile("build.zig.zon");
    var buffer: [10 * build_zig_zon.len]u8 = undefined;
    var fba = std.heap.FixedBufferAllocator.init(&buffer);
    const version = std.zon.parse.fromSlice(
        struct { version: []const u8 },
        fba.allocator(),
        build_zig_zon,
        null,
        .{ .ignore_unknown_fields = true },
    ) catch @panic("Invalid build.zig.zon!");
    return version.version;
}
