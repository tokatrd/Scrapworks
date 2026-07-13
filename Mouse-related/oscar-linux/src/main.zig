//! oscar — Linux-native replacement for A4Tech/Bloody Oscar Editor.
//!
//! "As low level as possible" — Zig with direct libusb HID control.
//!
//! Usage:
//!   oscar list-devices          Enumerate connected Oscar mice
//!   oscar info                  Read device status (read-only)
//!   oscar probe                 Probe HID feature reports (read-only)
//!   oscar profile read          Read current DPI profile
//!   oscar profile write <dpi>   Write DPI stages (comma-separated)
//!   oscar eeprom read <addr>    Read 16-bit value from EEPROM
//!   oscar eeprom write <addr> <val>  Write 16-bit value to EEPROM
//!   oscar ini parse <file>      Parse an Oscar INI config file
//!   oscar help                  Show this help

const std = @import("std");
const protocol = @import("protocol.zig");
const hid = @import("hid.zig");
const hidraw = @import("hidraw.zig");
const ini = @import("ini.zig");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        return printUsage(args[0]);
    }

    const cmd = args[1];

    // Commands that don't need a device
    if (std.mem.eql(u8, cmd, "help") or std.mem.eql(u8, cmd, "--help") or std.mem.eql(u8, cmd, "-h")) {
        return printUsage(args[0]);
    }

    if (std.mem.eql(u8, cmd, "ini") and args.len >= 4 and std.mem.eql(u8, args[2], "parse")) {
        return cmdIniParse(allocator, args[3]);
    }

    if (std.mem.eql(u8, cmd, "list-devices")) {
        return cmdListDevices(allocator);
    }

    // Open first Oscar-compatible device
    const ctx = try hid.init();
    defer hid.deinit(ctx);

    var device = try openFirstDevice(ctx, allocator) orelse {
        std.debug.print("No Oscar-compatible device found. Connect a mouse and try again.\n", .{});
        std.process.exit(1);
    };
    defer hid.closeDevice(&device);

    if (std.mem.eql(u8, cmd, "info")) {
        return cmdInfo(&device);
    } else if (std.mem.eql(u8, cmd, "probe")) {
        return cmdProbe(&device);
    } else if (std.mem.eql(u8, cmd, "profile")) {
        if (args.len < 3) {
            std.debug.print("usage: oscar profile read|write [dpi,...]\n", .{});
            return;
        }
        if (std.mem.eql(u8, args[2], "read")) {
            return cmdProfileRead(&device);
        } else if (std.mem.eql(u8, args[2], "write")) {
            if (args.len < 4) {
                std.debug.print("usage: oscar profile write <dpi1,dpi2,...>\n", .{});
                return;
            }
            return cmdProfileWrite(allocator, &device, args[3]);
        }
    } else if (std.mem.eql(u8, cmd, "eeprom")) {
        if (args.len < 4) {
            std.debug.print("usage: oscar eeprom read <addr> | write <addr> <value>\n", .{});
            return;
        }
        if (std.mem.eql(u8, args[2], "read")) {
            const addr = try std.fmt.parseInt(u16, args[3], 16);
            return cmdEepromRead(&device, addr);
        } else if (std.mem.eql(u8, args[2], "write")) {
            if (args.len < 5) {
                std.debug.print("usage: oscar eeprom write <addr> <value>\n", .{});
                return;
            }
            const addr = try std.fmt.parseInt(u16, args[3], 16);
            const val = try std.fmt.parseInt(u16, args[4], 16);
            return cmdEepromWrite(&device, addr, val);
        }
    }

    std.debug.print("Unknown command: {s}\n", .{cmd});
    return printUsage(args[0]);
}

fn printUsage(argv0: []const u8) !void {
    const stderr = std.io.getStdErr().writer();
    try stderr.print(
        \\oscar — Oscar Editor Linux replacement
        \\
        \\Usage: {s} <command> [args]
        \\
        \\Device commands:
        \\  list-devices          List connected Oscar-compatible mice
        \\  info                  Read device status (safe, read-only)
        \\  probe                 Probe HID feature report protocol (read-only)
        \\  profile read          Read current DPI profile from device
        \\  profile write <dpi>   Write DPI profile (comma-sep, e.g. "800,1600,3200")
        \\
        \\EEPROM commands (DANGER: may corrupt device):
        \\  eeprom read <addr>    Read 16-bit value from EEPROM (hex addr)
        \\  eeprom write <addr> <val>  Write 16-bit to EEPROM (hex)
        \\
        \\Offline commands:
        \\  ini parse <file>      Parse and dump an Oscar INI config
        \\  help                  Show this help
        \\
        \\Known devices (VID:PID):
        \\
    , .{argv0});
    for (protocol.known_devices) |d| {
        try stderr.print("  {x:0>4}:{x:0>4}  {s}\n", .{ d.vid, d.pid, d.name });
    }
    try stderr.print(
        \\
        \\Built with Zig {s} — zero runtime, manual memory, raw libusb.
        \\
    , .{@tagName(@import("builtin").target.cpu.arch)});
}

/// Open the first Oscar-compatible device found.
fn openFirstDevice(ctx: *hid.c.libusb_context, allocator: std.mem.Allocator) !?hid.DeviceHandle {
    const devices = hid.enumerate(ctx, allocator) catch |err| {
        switch (err) {
            error.InitFailed => std.debug.print("Failed to initialize libusb.\n", .{}),
            else => std.debug.print("Error enumerating devices: {}\n", .{err}),
        }
        return null;
    };
    defer allocator.free(devices);

    if (devices.len == 0) return null;
    return devices[0];
}

fn cmdListDevices(allocator: std.mem.Allocator) !void {
    const ctx = try hid.init();
    defer hid.deinit(ctx);

    const devices = try hid.enumerate(ctx, allocator);
    defer allocator.free(devices);

    if (devices.len == 0) {
        std.debug.print("No Oscar-compatible devices found.\n", .{});
        std.debug.print("Make sure your mouse is connected and you have udev rules for HID access.\n", .{});
        return;
    }

    std.debug.print("Found {d} Oscar device(s):\n\n", .{devices.len});
    for (devices, 0..) |*dev, i| {
        var name: []const u8 = "Unknown Oscar device";
        for (protocol.known_devices) |known| {
            if (dev.descriptor.idVendor == known.vid and dev.descriptor.idProduct == known.pid) {
                name = known.name;
                break;
            }
        }
        std.debug.print("  [{d}] VID:PID = {x:0>4}:{x:0>4}  {s}\n", .{
            i, dev.descriptor.idVendor, dev.descriptor.idProduct, name,
        });
    }
    std.debug.print("\nUse 'oscar info' to query a device.\n", .{});
}

fn cmdInfo(dev: *hid.DeviceHandle) !void {
    // Dump raw response bytes to understand protocol
    var raw_buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
    raw_buf[0] = @intFromEnum(protocol.Cmd.read_status);
    _ = dev.setFeature(&raw_buf) catch {};
    @memset(&raw_buf, 0);
    raw_buf[0] = @intFromEnum(protocol.Cmd.read_status);
    try dev.getFeature(&raw_buf);

    std.debug.print("Device Status (raw):\n", .{});
    std.debug.print("  Response ({d} bytes): ", .{protocol.report_size});
    for (raw_buf, 0..) |b, i| {
        if (i >= 16) break;
        std.debug.print("{x:0>2} ", .{b});
    }
    std.debug.print("\n\n", .{});

    // Try parsing fields
    std.debug.print("Parsed:\n", .{});
    std.debug.print("  Header:      0x{x:0>2}\n", .{raw_buf[0]});
    const fw = (@as(u16, raw_buf[1]) << 8) | raw_buf[2];
    std.debug.print("  Firmware:    v{}.{}\n", .{ fw >> 8, fw & 0xFF });
    std.debug.print("  Battery:     {d} (byte 3)\n", .{raw_buf[3]});
    std.debug.print("  Profile:     {d}\n", .{raw_buf[4]});
    std.debug.print("  DPI Stage:   {d}\n", .{raw_buf[5]});
    std.debug.print("  Rate:        0x{x:0>4} (bytes 6-7)\n", .{(@as(u16, raw_buf[6]) << 8) | raw_buf[7]});
    std.debug.print("  Extra:       ", .{});
    for (raw_buf[8..16]) |b| {
        std.debug.print("{x:0>2} ", .{b});
    }
    std.debug.print("\n", .{});
}

fn cmdProbe(dev: *hid.DeviceHandle) !void {
    std.debug.print("=== Oscar HID Protocol Probe ===\n", .{});
    std.debug.print("Device: {x:0>4}:{x:0>4} (interface {d})\n\n", .{
        dev.descriptor.idVendor, dev.descriptor.idProduct, dev.interface,
    });

    // Probe 1: Try all report IDs 1-16 with getFeature only (safest)
    std.debug.print("--- Probe 1: getFeature with report IDs 1-16 ---\n", .{});
    for (0..16) |rid_raw| {
        const rid = @as(u8, @intCast(rid_raw + 1));
        var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
        buf[0] = rid;
        const rc = dev.getFeature(&buf) catch continue;
        _ = rc;
        std.debug.print("  Report ID 0x{x:0>2}: ", .{rid});
        for (buf[0..8]) |b| {
            std.debug.print("{x:0>2} ", .{b});
        }
        std.debug.print("\n", .{});
    }

    // Probe 2: Try read_eeprom command at safe addresses (device info range only)
    std.debug.print("\n--- Probe 2: read_eeprom at device info range ---\n", .{});
    const probe_addrs = [_]u16{ 0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x0010, 0x0100, 0x0102 };
    for (probe_addrs) |addr| {
        var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
        buf[0] = @intFromEnum(protocol.Cmd.read_eeprom);
        buf[1] = @intCast((addr >> 8) & 0xFF);
        buf[2] = @intCast(addr & 0xFF);
        _ = dev.setFeature(&buf) catch {};
        @memset(&buf, 0);
        buf[0] = @intFromEnum(protocol.Cmd.read_eeprom);
        const rc = dev.getFeature(&buf) catch {
            std.debug.print("  EEPROM[0x{x:0>4}]: getFeature FAILED\n", .{addr});
            continue;
        };
        _ = rc;
        std.debug.print("  EEPROM[0x{x:0>4}]: ", .{addr});
        for (buf[0..8]) |b| {
            std.debug.print("{x:0>2} ", .{b});
        }
        std.debug.print("\n", .{});
    }
}

fn cmdProfileRead(dev: *hid.DeviceHandle) !void {
    std.debug.print("Reading DPI profile from device...\n\n", .{});
    for (0..6) |i| {
        const addr: u16 = @intCast(protocol.eeprom.dpi_profiles.start + i * 2);
        const val = dev.readEeprom(addr) catch |err| {
            std.debug.print("  Stage {d}: read error: {}\n", .{ i, err });
            continue;
        };
        const enabled = (val & 0x8000) != 0;
        const dpi = val & 0x7FFF;
        if (dpi == 0) continue;
        std.debug.print("  Stage {d}: {s} {d} DPI\n", .{ i, if (enabled) "ENABLED" else "DISABLED", dpi });
    }
}

/// Write to EEPROM, trying hidraw first (for devices where libusb
/// setFeature times out, e.g. PID 9066). Falls back to libusb.
fn writeEeprom16WithFallback(dev: *hid.DeviceHandle, addr: u16, val: u16) !void {
    // Try hidraw first — routes through kernel HID stack (like Windows HID API)
    if (hidraw.openByVidPid(@intCast(dev.descriptor.idVendor), @intCast(dev.descriptor.idProduct))) |hdev| {
        defer hdev.close();
        var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
        buf[0] = @intFromEnum(protocol.Cmd.write_eeprom);
        buf[1] = @intCast((addr >> 8) & 0xFF);
        buf[2] = @intCast(addr & 0xFF);
        buf[3] = @intCast((val >> 8) & 0xFF);
        buf[4] = @intCast(val & 0xFF);
        if (hidraw.setFeature(hdev, &buf)) |_| {
            return; // hidraw succeeded
        } else |err| {
            std.debug.print("  hidraw write failed ({}), falling back to libusb...\n", .{err});
        }
    } else |_| {
        // hidraw not available, fall through to libusb
    }

    // Fallback: libusb (setFeature times out on PID 9066, but device may still process it)
    dev.writeEeprom16(addr, val) catch |err| {
        std.debug.print("  libusb write: {} (device may have received it)\n", .{err});
        return;
    };
}

fn cmdProfileWrite(allocator: std.mem.Allocator, dev: *hid.DeviceHandle, dpi_list: []const u8) !void {
    var stages = std.ArrayList(struct { dpi: u16, enabled: bool }).init(allocator);
    defer stages.deinit();

    var it = std.mem.splitScalar(u8, dpi_list, ',');
    while (it.next()) |token| {
        const trimmed = std.mem.trim(u8, token, " ");
        if (trimmed.len == 0) continue;
        const dpi = std.fmt.parseInt(u16, trimmed, 10) catch {
            std.debug.print("Invalid DPI value: '{s}'\n", .{trimmed});
            return;
        };
        try stages.append(.{ .dpi = dpi, .enabled = true });
    }

    if (stages.items.len == 0) {
        std.debug.print("No DPI values provided.\n", .{});
        return;
    }

    std.debug.print("Writing {d} DPI stages to device...\n", .{stages.items.len});
    for (stages.items, 0..) |stage, i| {
        const addr: u16 = @intCast(protocol.eeprom.dpi_profiles.start + i * 2);
        const val: u16 = (if (stage.enabled) @as(u16, 0x8000) else 0) | (stage.dpi & 0x7FFF);
        writeEeprom16WithFallback(dev, addr, val) catch |err| {
            std.debug.print("  Stage {d} ({d} DPI): write error: {}\n", .{ i, stage.dpi, err });
            return;
        };
        std.debug.print("  Stage {d}: {d} DPI written\n", .{ i, stage.dpi });
    }
    std.debug.print("Profile write complete.\n", .{});
}

fn cmdEepromRead(dev: *hid.DeviceHandle, addr: u16) !void {
    const val = dev.readEeprom(addr) catch |err| {
        std.debug.print("EEPROM read failed at 0x{x:0>4}: {}\n", .{ addr, err });
        return;
    };
    std.debug.print("EEPROM[0x{x:0>4}] = 0x{x:0>4} ({d})\n", .{ addr, val, val });
}

fn cmdEepromWrite(dev: *hid.DeviceHandle, addr: u16, val: u16) !void {
    try writeEeprom16WithFallback(dev, addr, val);
    std.debug.print("EEPROM[0x{x:0>4}] = 0x{x:0>4} — written\n", .{ addr, val });
}

fn cmdIniParse(allocator: std.mem.Allocator, filepath: []const u8) !void {
    const file = std.fs.cwd().openFile(filepath, .{}) catch |err| {
        std.debug.print("Failed to open '{s}': {}\n", .{ filepath, err });
        return;
    };
    defer file.close();

    const data = try file.readToEndAlloc(allocator, 1024 * 1024);
    defer allocator.free(data);

    var parser = try ini.IniParser.init(allocator, data);
    defer parser.deinit();
    const entries = try parser.parse();
    defer {
        for (entries.items) |e| {
            allocator.free(e.section);
            allocator.free(e.key);
        }
        entries.deinit();
    }

    std.debug.print("INI File: {s}\n", .{filepath});
    std.debug.print("Entries: {d}\n\n", .{entries.items.len});

    var current_section: ?[]const u8 = null;
    for (entries.items) |entry| {
        if (current_section == null or !std.mem.eql(u8, current_section.?, entry.section)) {
            current_section = entry.section;
            std.debug.print("[{s}]\n", .{entry.section});
        }
        const val_str = switch (entry.value) {
            .string => |s| s,
            .int => |i| std.fmt.allocPrint(allocator, "{d}", .{i}) catch "?",
            .bool => |b| if (b) "true" else "false",
        };
        std.debug.print("  {s} = {s}\n", .{ entry.key, val_str });
    }
}
