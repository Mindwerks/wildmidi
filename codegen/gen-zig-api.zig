const std = @import("std");
const case = @import("case");
const cli = @import("cli");
const zigft = @import("zigft");
const api_translator = zigft.api_translator;

const CliConfig = struct {
    outpath: []const u8,
};

var config = CliConfig{
    .outpath = undefined,
};

pub fn main() !void {
    var r = try cli.AppRunner.init(std.heap.page_allocator);

    const app = cli.App{
        .command = .{
            .name = "codegen",
            .options = try r.allocOptions(&.{
                cli.Option{
                    .long_name = "outpath",
                    .help = "Output path for generated bindings",
                    .required = true,
                    .value_ref = r.mkRef(&config.outpath),
                },
            }),
            .target = .{
                .action = .{
                    .exec = generate,
                },
            },
        },
    };

    return r.run(&app);
}

fn generate() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();

    const allocator = gpa.allocator();

    var generator: *api_translator.CodeGenerator(.{
        .include_paths = &[_][]const u8{
            "include/",
            "src/",
        },
        .header_paths = &[_][]const u8{
            "wildmidi_lib.h",
            "wm_error.h",
        },
        .c_error_type = "int",
        .c_root_struct = "midi",
        .error_enum = "ErrorEnum",
        .filter_fn = filter,
        .param_override_fn = overrideParam,
        .param_is_slice_len_fn = paramIsSliceLen,
        .type_name_fn = getTypeName,
        .fn_name_fn = getFnName,
        .enum_name_fn = getEnumName,
        .error_name_fn = getErrorName,
        .const_is_enum_item_fn = isEnumItem,
    }) = try .init(allocator);
    defer generator.deinit();

    // analyze the headers
    try generator.analyze();

    // save translated code to file
    var file = try std.fs.createFileAbsolute(config.outpath, .{});
    try generator.print(file.writer());

    file.close();
}

const camelize = api_translator.camelize;
const snakify = api_translator.snakify;

const prefixes = .{
    "WildMidi_",
    "_WM",
};

const enum_types = .{
    .{
        .prefix = "WM_ERR_",
        .type = api_translator.EnumInfo{ .name = "ErrorEnum", .tag_type = "c_int" },
    },
    .{
        .prefix = "WM_MO_",
        .type = api_translator.EnumInfo{ .name = "MixerOptions", .tag_type = "u16", .is_packed_struct = true },
    },
};

const type_overrides = .{
    .mixer_options = "MixerOptions",
};

fn getNameOffset(name: []const u8) usize {
    return inline for (prefixes) |prefix| {
        if (std.mem.startsWith(u8, name, prefix)) break prefix.len;
    } else 0;
}

fn filter(name: []const u8) bool {
    return getNameOffset(name) > 0;
}

fn overrideParam(_: []const u8, param_name: ?[]const u8, _: usize, _: []const u8) ?[]const u8 {
    return inline for (std.meta.fields(@TypeOf(type_overrides))) |field| {
        if (param_name) |name| {
            if (std.mem.eql(u8, name, field.name)) break @field(type_overrides, field.name);
        }
    } else null;
}

fn isEnumItem(name: []const u8) ?api_translator.EnumInfo {
    return inline for (enum_types) |t| {
        if (std.mem.startsWith(u8, name, t.prefix)) break comptime t.type;
    } else null;
}

fn getFnName(allocator: std.mem.Allocator, name: []const u8) ![]const u8 {
    return camelize(allocator, name, getNameOffset(name), false);
}

fn getTypeName(allocator: std.mem.Allocator, name: []const u8) ![]const u8 {
    return camelize(allocator, name, getNameOffset(name), true);
}

fn getEnumName(allocator: std.mem.Allocator, name: []const u8) ![]const u8 {
    return inline for (enum_types) |t| {
        if (std.mem.startsWith(u8, name, t.prefix))
            break snakify(allocator, name, t.prefix.len);
    } else name;
}

fn getErrorName(allocator: std.mem.Allocator, name: []const u8) ![]const u8 {
    return camelize(allocator, name, 0, true);
}

const slice_len_fns = .{
    "WildMidi_OpenBuffer",
    "WildMidi_GetMidiOutput",
    "WildMidi_GetOutput",
    "WildMidi_ConvertToMidi",
    "WildMidi_ConvertBufferToMidi",
};

const slice_len_arg_names = .{
    "size",
    "insize",
};

fn paramIsSliceLen(fn_name: []const u8, param_name: ?[]const u8, param_index: usize, param_type: []const u8) ?usize {
    _ = param_type;

    var found = false;
    inline for (slice_len_fns) |match| {
        if (std.mem.eql(u8, fn_name, match)) {
            found = true;
        }
    }

    if (!found) return null;

    found = false;

    inline for (slice_len_arg_names) |arg| {
        if (std.mem.eql(u8, param_name.?, arg)) {
            found = true;
        }
    }

    return if (found) param_index - 1 else null;
}
