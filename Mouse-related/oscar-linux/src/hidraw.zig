//! HID Feature Report I/O via hidraw (/dev/hidraw*).
//!
//! Uses HIDIOCSFEATURE / HIDIOCGFEATURE ioctls through the kernel HID
//! subsystem. This is the Linux equivalent of Windows' HidD_SetFeature /
//! HidD_GetFeature and does NOT require detaching the kernel driver.
//!
//! Use this as a fallback when libusb setFeature times out (as seen on
//! X7 F3 V-Track PID 9066, where the MCU goes busy during EEPROM writes).

const std = @import("std");
const protocol = @import("protocol.zig");

pub const Error = error{
    DeviceNotFound,
    OpenFailed,
    TransferFailed,
    PermissionDenied,
};

// ioctl request codes (from linux/hidraw.h on x86_64)
const HIDIOCGRAWINFO: u32 = 0x80084803;
const HIDIOCSFEATURE: u32 = 0xc0404806; // len=64
const HIDIOCGFEATURE: u32 = 0xc0404807; // len=64

/// Raw device info from HIDIOCGRAWINFO ioctl.
const HidrawDevInfo = extern struct {
    bustype: u32,
    vendor: i16,
    product: i16,
};

/// Open a hidraw device by VID:PID match.
/// Scans /dev/hidraw* and returns the first matching device file.
pub fn openByVidPid(vid: u16, pid: u16) Error!std.fs.File {
    var buf: [64]u8 = undefined;

    for (0..32) |i| {
        const path = std.fmt.bufPrint(&buf, "/dev/hidraw{d}", .{i}) catch continue;
        const fd = std.fs.openFileAbsolute(path, .{ .mode = .read_write }) catch continue;
        defer fd.close();

        // Get device info via ioctl
        var info: HidrawDevInfo = undefined;
        const rc = std.os.linux.ioctl(fd.handle, HIDIOCGRAWINFO, @intFromPtr(&info));
        if (rc < 0) continue;

        if (info.vendor == @as(i16, @intCast(vid)) and info.product == @as(i16, @intCast(pid))) {
            // Re-open and return
            return std.fs.openFileAbsolute(path, .{ .mode = .read_write }) catch return Error.OpenFailed;
        }
    }

    return Error.DeviceNotFound;
}

/// Send a HID Feature Report (write) — equivalent to HidD_SetFeature.
pub fn setFeature(fd: std.fs.File, report: []const u8) Error!void {
    var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
    @memcpy(buf[0..@min(report.len, protocol.report_size)], report);

    const rc = std.os.linux.ioctl(fd.handle, HIDIOCSFEATURE, @intFromPtr(&buf));
    if (rc < 0) {
        return switch (std.os.linux.errno(@as(i32, @intCast(-rc)))) {
            .PERM => Error.PermissionDenied,
            else => Error.TransferFailed,
        };
    }
}

/// Receive a HID Feature Report (read) — equivalent to HidD_GetFeature.
pub fn getFeature(fd: std.fs.File, report: []u8) Error!void {
    var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
    if (report.len > 0) buf[0] = report[0];

    const rc = std.os.linux.ioctl(fd.handle, HIDIOCGFEATURE, @intFromPtr(&buf));
    if (rc < 0) {
        return switch (std.os.linux.errno(@as(i32, @intCast(-rc)))) {
            .PERM => Error.PermissionDenied,
            else => Error.TransferFailed,
        };
    }

    // Copy response back
    const count = @min(@as(usize, @intCast(rc)), report.len);
    @memcpy(report[0..count], buf[0..count]);
}
