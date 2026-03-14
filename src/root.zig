const WildMidiErrorCode = enum(c_int) {
    no_error = 0,
    unable_to_allocate_memory,
    unable_to_stat,
    unable_to_load,
    unable_to_open,
    unable_to_read,
    invalid_or_unsupported_file_format,
    file_corrupt,
    library_not_initialized,
    invalid_argument,
    library_already_initialized,
    not_a_midi_file,
    refusing_to_load_unusually_long_file,
    not_an_hmp_file,
    not_an_hmi_file,
    unable_to_convert,
    not_a_mus_file,
    not_an_xmi_file,
    invalid_error_code,
};

pub const WildMidiError = error{
    NoError,
    UnableToAllocateMemory,
    UnableToStat,
    UnableToLoad,
    UnableToOpen,
    UnableToRead,
    InvalidOrUnsupportedFileFormat,
    FileCorrupt,
    LibraryNotInitialized,
    InvalidArgument,
    LibraryAlreadyInitialized,
    NotAMidiFile,
    RefusingToLoadUnusuallyLongFile,
    NotAnHmpFile,
    NotAnHmiFile,
    UnableToConvert,
    NotAMusFile,
    NotAnXmiFile,
    InvalidErrorCode,
};

fn handleError(error_status: c_int) WildMidiError!void {
    if (error_status != 0) {
        const error_code: WildMidiErrorCode = @enumFromInt(_WM_Global_ErrorI);
        switch (error_code) {
            .no_error => return,
            .unable_to_allocate_memory => return WildMidiError.UnableToAllocateMemory,
            .unable_to_stat => return WildMidiError.UnableToStat,
            .unable_to_load => return WildMidiError.UnableToLoad,
            .unable_to_open => return WildMidiError.UnableToOpen,
            .unable_to_read => return WildMidiError.UnableToRead,
            .invalid_or_unsupported_file_format => return WildMidiError.InvalidOrUnsupportedFileFormat,
            .file_corrupt => return WildMidiError.FileCorrupt,
            .library_not_initialized => return WildMidiError.LibraryNotInitialized,
            .invalid_argument => return WildMidiError.InvalidArgument,
            .library_already_initialized => return WildMidiError.LibraryAlreadyInitialized,
            .not_a_midi_file => return WildMidiError.NotAMidiFile,
            .refusing_to_load_unusually_long_file => return WildMidiError.RefusingToLoadUnusuallyLongFile,
            .not_an_hmp_file => return WildMidiError.NotAnHmpFile,
            .not_an_hmi_file => return WildMidiError.NotAnHmiFile,
            .unable_to_convert => return WildMidiError.UnableToConvert,
            .not_a_mus_file => return WildMidiError.NotAMusFile,
            .not_an_xmi_file => return WildMidiError.NotAnXmiFile,
            .invalid_error_code => return WildMidiError.InvalidErrorCode,
        }
    }
}

fn handleGlobalError() WildMidiError!void {
    try handleError(_WM_Global_ErrorI);
}

pub fn init(config_file: [:0]const u8, rate: u16, options: InitOptions) !void {
    return try handleError(WildMidi_Init(config_file, rate, options));
}

pub fn getVersion() c_long {
    return WildMidi_GetVersion();
}

pub const MidiFile = struct {
    handle: ?*anyopaque,

    pub fn open(midi_file: [:0]const u8) WildMidiError!MidiFile {
        const handle = WildMidi_Open(midi_file);

        try handleGlobalError();

        return MidiFile{
            .handle = handle,
        };
    }

    pub fn getOutput(self: MidiFile, buffer: [*c]u8, size: usize) c_int {
        return WildMidi_GetOutput(@ptrCast(@alignCast(self.handle)), buffer, size);
    }

    pub fn getInfo(self: MidiFile) !?*WildMidiInfo {
        const info = WildMidi_GetInfo(@ptrCast(@alignCast(self.handle)));

        try handleGlobalError();

        return info;
    }

    pub fn setOption(self: MidiFile, options: c_uint, setting: c_uint) !void {
        return try handleError(WildMidi_SetOption(@ptrCast(@alignCast(self.handle)), options, setting));
    }

    pub fn close(self: MidiFile) void {
        _ = WildMidi_Close(@ptrCast(@alignCast(self.handle)));
    }
};

pub fn openBuffer(midi_buffer: [*c]const u8, size: c_uint) MidiFile {
    const handle = WildMidi_OpenBuffer(midi_buffer, size);

    return MidiFile{
        .handle = handle,
    };
}

pub fn getError() [*c]u8 {
    return WildMidi_GetError();
}

pub fn masterVolume(master_volume: c_short) !void {
    return try handleError(WildMidi_MasterVolume(master_volume));
}

pub const InitOptions = enum(c_uint) {
    default = 0x0,
    log_volume = 0x1,
    enhanced_resampling = 0x2,
    reverb = 0x4,
    loop = 0x8,
    save_as_type0 = 0x1000,
    round_tempo = 0x2000,
    strip_silence = 0x4000,
    text_as_lyric = 0x8000,
};

pub const WildMidiInfo = extern struct {
    copyright: [*c]u8,
    current_sample: c_uint,
    approx_total_samples: c_uint,
    mixer_options: c_ushort,
    total_midi_time: c_uint,
};

extern fn WildMidi_Init(config_file: [*c]const u8, rate: u16, options: InitOptions) c_int;
extern fn WildMidi_GetVersion() c_long;
extern fn WildMidi_Open(midi_file: [*c]const u8) [*c]c_long;
extern fn WildMidi_OpenBuffer(midi_buffer: [*c]const u8, size: c_uint) [*c]c_long;
extern fn WildMidi_Close(handle: [*]c_long) c_int;
extern fn WildMidi_GetOutput(handle: [*c]c_long, buffer: [*c]u8, size: usize) c_int;
extern fn WildMidi_GetInfo(handle: [*]c_long) [*c]WildMidiInfo;
extern fn WildMidi_GetError() [*c]u8;
extern fn WildMidi_MasterVolume(master_volume: c_short) c_int;
extern fn WildMidi_SetOption(handle: [*]c_long, options: c_uint, setting: c_uint) c_int;

extern const _WM_Global_ErrorI: c_int;

const std = @import("std");
const StaticStringMap = std.StaticStringMap;
const logger = std.log.scoped(.wildmidi);
const testing = std.testing;

test {
    const freepats_path = try std.process.getEnvVarOwned(testing.allocator, "FREEPATS_PATH");
    defer testing.allocator.free(freepats_path);

    const freepats_config = try std.fs.path.joinZ(testing.allocator, &[_][]const u8{
        freepats_path,
        "freepats.cfg",
    });
    defer testing.allocator.free(freepats_config);

    const rate = 44100;
    const options: InitOptions = .default;

    try init(freepats_config, rate, options);

    const version = getVersion();
    try testing.expect(version != 0);

    const midi_file_name = "test/test.mid";
    const midi_file: MidiFile = MidiFile.open(midi_file_name) catch |err| {
        std.debug.print("Error opening MIDI file: {s}\n", .{getError()});
        return err;
    };
    defer midi_file.close();

    try testing.expect(midi_file.handle != null);

    if (try midi_file.getInfo()) |info| {
        std.debug.print("MIDI file info:\n", .{});
        if (info.copyright) |copyright| {
            std.debug.print("  Copyright: {s}\n", .{copyright});
        }
        std.debug.print("  Approx. total samples: {d}\n", .{info.approx_total_samples});
    }

    try masterVolume(100);
}
