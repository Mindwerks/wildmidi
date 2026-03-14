const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    lib_mod.addIncludePath(b.path("include"));

    const lib = b.addLibrary(.{
        .name = "wildmidi",
        .root_module = lib_mod,
    });

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
        "-DWILDMIDI_BUILD",
    };

    const config_header = b.addConfigHeader(
        .{
            .style = .{
                .cmake = b.path("include/config.h.cmake"),
            },
        },
        .{
            .WILDMIDI_CFG = b.pathFromRoot("cfg/wildmidi.cfg"),
            .WILDMIDI_VERSION = "0.4.6",
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

    lib_mod.addConfigHeader(config_header);

    switch (target.result.os.tag) {
        .windows => {
            lib_mod.addCSourceFiles(.{
                .files = &source_files,
                .flags = &(.{"-DWILDMIDI_STATIC"} ++ defaultFlags),
            });
            lib_mod.addIncludePath(b.path("mingw"));
        },
        .macos => {
            lib_mod.addCSourceFiles(.{
                .files = &source_files,
                .flags = &defaultFlags,
            });
            lib_mod.addIncludePath(b.path("macosx"));
        },
        else => {
            lib_mod.addCSourceFiles(.{
                .files = &source_files,
                .flags = &defaultFlags,
            });
        },
    }

    lib.installHeadersDirectory(b.path("include"), "", .{});

    b.installArtifact(lib);

    const mod = b.addModule("wildmidi", .{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    const tests = b.addTest(.{
        .root_module = mod,
    });

    tests.linkLibrary(lib);

    const freepats = b.dependency("freepats", .{});

    const run_lib_unit_tests = b.addRunArtifact(tests);
    run_lib_unit_tests.setEnvironmentVariable("FREEPATS_PATH", freepats.builder.build_root.path.?);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_lib_unit_tests.step);
}
