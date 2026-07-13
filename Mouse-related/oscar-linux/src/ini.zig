//! INI file parser for OscarEditor.exe configuration files.
//!
//! Parses the INI format used by the original OscarEditor.exe:
//! - Sections: [SectionName]
//! - Keys: KeyName=Value
//! - Comments: // at line start
//! - Unicode (UTF-16LE) encoding compatibility handled

const std = @import("std");

pub const IniValue = union(enum) {
    string: []const u8,
    int: i64,
    bool: bool,

    pub fn asString(self: IniValue, allocator: std.mem.Allocator) ![]const u8 {
        return switch (self) {
            .string => |s| try allocator.dupe(u8, s),
            .int => |i| std.fmt.allocPrint(allocator, "{d}", .{i}),
            .bool => |b| if (b) "1" else "0",
        };
    }
};

pub const IniParser = struct {
    allocator: std.mem.Allocator,
    data: []const u8,
    pos: usize,
    current_section: ?[]const u8,
    owned_data: bool,

    /// Convert UTF-16LE to ASCII by stripping null bytes.
    /// Borland C++ Builder saves INI files as UTF-16LE by default.
    fn convertUtf16Le(data: []const u8, allocator: std.mem.Allocator) ![]const u8 {
        if (data.len < 2) return data;
        // Check for UTF-16LE BOM (0xFF 0xFE) or null-byte pattern
        const is_utf16 = (data.len >= 2 and data[0] == 0xFF and data[1] == 0xFE) or
            (data.len >= 4 and data[0] != 0 and data[1] == 0 and data[2] != 0 and data[3] == 0);

        if (!is_utf16) return data;

        const start: usize = if (data[0] == 0xFF and data[1] == 0xFE) 2 else 0;
        var result = try std.ArrayList(u8).initCapacity(allocator, data.len / 2);
        var i = start;
        while (i + 1 < data.len) : (i += 2) {
            // Only take ASCII-range characters (low byte when high byte is 0)
            if (data[i + 1] == 0 and data[i] != 0) {
                try result.append(data[i]);
            } else if (data[i] == '\n') {
                // Handle \r\n vs \n — skip \r in UTF-16LE too
                try result.append('\n');
            } else if (data[i] == '\r') {
                // skip
            }
        }
        return result.toOwnedSlice();
    }

    pub fn init(allocator: std.mem.Allocator, data: []const u8) !IniParser {
        const converted = try convertUtf16Le(data, allocator);
        const owned = converted.ptr != data.ptr;
        return .{
            .allocator = allocator,
            .data = converted,
            .pos = 0,
            .current_section = null,
            .owned_data = owned,
        };
    }

    pub fn deinit(parser: *IniParser) void {
        if (parser.owned_data) {
            parser.allocator.free(parser.data);
        }
    }

    pub const Entry = struct {
        section: []const u8,
        key: []const u8,
        value: IniValue,
    };

    /// Parse the INI data, returning all entries.
    pub fn parse(parser: *IniParser) !std.ArrayList(Entry) {
        var entries = std.ArrayList(Entry).init(parser.allocator);

        while (parser.pos < parser.data.len) {
            parser.skipWhitespace();
            if (parser.pos >= parser.data.len) break;

            const c = parser.data[parser.pos];

            if (c == '[') {
                // Section header
                parser.pos += 1;
                const section_start = parser.pos;
                while (parser.pos < parser.data.len and parser.data[parser.pos] != ']') : (parser.pos += 1) {}
                if (parser.pos >= parser.data.len) break;
                parser.current_section = parser.data[section_start..parser.pos];
                parser.pos += 1; // skip ]
                _ = parser.skipLine();
            } else if (c == '/' and parser.pos + 1 < parser.data.len and parser.data[parser.pos + 1] == '/') {
                // Comment line
                _ = parser.skipLine();
            } else if (c == '\r' or c == '\n') {
                parser.pos += 1;
            } else {
                // Key=Value line
                const key_start = parser.pos;
                while (parser.pos < parser.data.len and parser.data[parser.pos] != '=') : (parser.pos += 1) {}
                const key = std.mem.trim(u8, parser.data[key_start..parser.pos], " \t\r\n");
                if (parser.pos < parser.data.len) parser.pos += 1; // skip =
                parser.skipWhitespace();

                const val_start = parser.pos;
                _ = parser.skipLine();
                const val_end = parser.pos;

                // Trim trailing whitespace from value
                var val = parser.data[val_start..val_end];
                val = std.mem.trimRight(u8, val, " \t\r\n");

                const section = parser.current_section orelse "";

                // Determine value type
                const entry_value: IniValue = if (std.ascii.eqlIgnoreCase(val, "true") or std.ascii.eqlIgnoreCase(val, "yes") or std.mem.eql(u8, val, "1"))
                    .{ .bool = true }
                else if (std.ascii.eqlIgnoreCase(val, "false") or std.ascii.eqlIgnoreCase(val, "no") or std.mem.eql(u8, val, "0"))
                    .{ .bool = false }
                else if (std.fmt.parseInt(i64, val, 10)) |int_val|
                    .{ .int = int_val }
                else |_|
                    .{ .string = val };

                try entries.append(.{
                    .section = try parser.allocator.dupe(u8, section),
                    .key = try parser.allocator.dupe(u8, key),
                    .value = entry_value,
                });
            }
        }

        return entries;
    }

    fn skipWhitespace(parser: *IniParser) void {
        while (parser.pos < parser.data.len and (parser.data[parser.pos] == ' ' or parser.data[parser.pos] == '\t')) {
            parser.pos += 1;
        }
    }

    fn skipLine(parser: *IniParser) usize {
        const start = parser.pos;
        while (parser.pos < parser.data.len and parser.data[parser.pos] != '\n') : (parser.pos += 1) {}
        if (parser.pos < parser.data.len) parser.pos += 1; // skip \n
        return parser.pos - start;
    }
};

/// Build a DPI profile from Main.ini section data.
pub fn parseDpiProfile(entries: []const IniParser.Entry, sensor_key: []const u8) !?protocol.DpiProfile {
    _ = sensor_key;
    // Look for DPI.X entries (e.g. DPI_0, DPI_1, ...)
    var stages: [6]protocol.DpiStage = [_]protocol.DpiStage{
        .{ .enabled = false, .dpi = 0 },
    } ** 6;
    var count: u6 = 0;

    for (entries) |entry| {
        if (std.mem.startsWith(u8, entry.key, "DPI_") and entry.key.len > 4) {
            const idx = std.fmt.parseInt(u8, entry.key[4..], 10) catch continue;
            if (idx >= 6) continue;

            // Value format: "Enable,1600" or "Disable,1600"
            const val_str = switch (entry.value) {
                .string => |s| s,
                else => continue,
            };

            var it = std.mem.splitScalar(u8, val_str, ',');
            const enabled_str = it.next() orelse continue;
            const dpi_str = it.next() orelse continue;

            stages[idx] = .{
                .enabled = std.ascii.eqlIgnoreCase(enabled_str, "Enable"),
                .dpi = std.fmt.parseInt(u16, std.mem.trim(u8, dpi_str, " "), 10) catch 0,
            };
            if (idx >= count) count = idx + 1;
        }
    }

    return protocol.DpiProfile{
        .stage_count = count,
        .stages = stages,
    };
}

const protocol = @import("protocol.zig");
