const std = @import("std");

pub fn Init(config_file: [*c]const u8, rate: u16, options: InitOptions) c_int {
    return WildMidi_Init(config_file, rate, options);
}

pub fn GetVersion() c_long {
    return WildMidi_GetVersion();
}

pub const MidiFile = struct {
    handle: [*c]c_long,

    pub fn getOutput(self: MidiFile, buffer: [*c]u8, size: usize) c_int {
        return WildMidi_GetOutput(self.handle, buffer, size);
    }

    pub fn getInfo(self: MidiFile) *WildMidiInfo {
        return WildMidi_GetInfo(self.handle);
    }

    pub fn setOption(self: MidiFile, options: c_uint, setting: c_uint) c_int {
        return WildMidi_SetOption(self.handle, options, setting);
    }

    pub fn close(self: MidiFile) void {
        _ = WildMidi_Close(self.handle);
    }
};

pub fn Open(midi_file: []const u8) !MidiFile {
    const midi_file_terminated = try std.mem.concat(std.heap.c_allocator, u8, &.{ midi_file, "\x00" });
    defer std.heap.c_allocator.free(midi_file_terminated);

    const handle = WildMidi_Open(@ptrCast([*c]const u8, midi_file_terminated));

    return MidiFile{
        .handle = handle,
    };
}

pub fn OpenBuffer(midi_buffer: [*c]const u8, size: c_uint) MidiFile {
    const handle = WildMidi_OpenBuffer(midi_buffer, size);

    return MidiFile{
        .handle = handle,
    };
}

pub fn GetError() [*c]u8 {
    return WildMidi_GetError();
}

pub fn MasterVolume(master_volume: c_short) c_int {
    return WildMidi_MasterVolume(master_volume);
}

pub const InitOptions = enum(c_uint) {
    Default = 0x0,
    LogVolume = 0x1,
    EnhancedResampling = 0x2,
    Reverb = 0x4,
    Loop = 0x8,
    SaveAsType0 = 0x1000,
    RoundTempo = 0x2000,
    StripSilence = 0x4000,
    TextAsLyric = 0x8000,
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
