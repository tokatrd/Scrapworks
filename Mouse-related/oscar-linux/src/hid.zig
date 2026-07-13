//! Low-level HID communication via libusb.
//!
//! Translates the Windows HID API calls (CreateFile, HidD_GetFeature,
//! HidD_SetFeature) used by the original OscarEditor.exe into Linux
//! equivalents via libusb control transfers.

const std = @import("std");
const protocol = @import("protocol.zig");

pub const c = @cImport({
    @cInclude("libusb-1.0/libusb.h");
});

pub const Error = error{
    InitFailed,
    DeviceNotFound,
    OpenFailed,
    ClaimFailed,
    TransferFailed,
    Timeout,
    NotSupported,
};

/// Initialize libusb context.
pub fn init() Error!*c.libusb_context {
    var ctx: ?*c.libusb_context = null;
    const rc = c.libusb_init(&ctx);
    if (rc < 0) {
        return Error.InitFailed;
    }
    return ctx orelse return Error.InitFailed;
}

/// Deinitialize libusb context.
pub fn deinit(ctx: *c.libusb_context) void {
    c.libusb_exit(ctx);
}

/// Represents an opened HID device handle.
pub const DeviceHandle = struct {
    handle: *c.libusb_device_handle,
    dev: *c.libusb_device,
    descriptor: c.libusb_device_descriptor,
    interface: c_int,
    ep_in: u8,
    ep_out: u8,
    chipset: protocol.Chipset,

    /// Send a HID Feature Report via control transfer.
    /// Corresponds to Windows HidD_SetFeature().
    pub fn setFeature(self: *DeviceHandle, report: []const u8) Error!void {
        // HID Set_Report (Feature) via control endpoint 0
        // bmRequestType: 0x21 (Host-to-Device, Class, Interface)
        // bRequest: 0x09 (SET_REPORT)
        // wValue: 0x0300 | report[0] (Feature = 0x03, ReportID = report[0])
        // wIndex: interface number
        const wValue: u16 = 0x0300 | @as(u16, report[0]);
        const rc = c.libusb_control_transfer(
            self.handle,
            0x21, // Host-to-Device | Class | Interface
            0x09, // SET_REPORT
            wValue,
            @as(u16, @intCast(self.interface)),
            @constCast(report.ptr),
            @as(u16, @intCast(report.len)),
            5000, // 5s timeout
        );
        if (rc < 0) {
            return Error.TransferFailed;
        }
    }

    /// Receive a HID Feature Report via control transfer.
    /// Corresponds to Windows HidD_GetFeature().
    pub fn getFeature(self: *DeviceHandle, report: []u8) Error!void {
        // HID Get_Report (Feature) via control endpoint 0
        // bmRequestType: 0xA1 (Device-to-Host, Class, Interface)
        // bRequest: 0x01 (GET_REPORT)
        // wValue: 0x0300 | report[0] (Feature = 0x03, ReportID)
        // wIndex: interface number
        const wValue: u16 = 0x0300 | @as(u16, report[0]);
        const rc = c.libusb_control_transfer(
            self.handle,
            0xA1, // Device-to-Host | Class | Interface
            0x01, // GET_REPORT
            wValue,
            @as(u16, @intCast(self.interface)),
            report.ptr,
            @as(u16, @intCast(report.len)),
            5000,
        );
        if (rc < 0) {
            return Error.TransferFailed;
        }
    }

    /// Read an interrupt IN transfer.
    pub fn readInterrupt(self: *DeviceHandle, buf: []u8) Error!usize {
        var actual: c_int = 0;
        const rc = c.libusb_interrupt_transfer(
            self.handle,
            self.ep_in,
            buf.ptr,
            @as(c_int, @intCast(buf.len)),
            &actual,
            5000,
        );
        if (rc < 0) return Error.TransferFailed;
        return @as(usize, @intCast(actual));
    }

    /// Write an interrupt OUT transfer.
    pub fn writeInterrupt(self: *DeviceHandle, data: []const u8) Error!void {
        var actual: c_int = 0;
        const rc = c.libusb_interrupt_transfer(
            self.handle,
            self.ep_out,
            @constCast(data.ptr),
            @as(c_int, @intCast(data.len)),
            &actual,
            5000,
        );
        if (rc < 0) return Error.TransferFailed;
    }

    /// Read EEPROM at given address (16-bit aligned).
    pub fn readEeprom(self: *DeviceHandle, address: u16) Error!u16 {
        var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
        buf[0] = @intFromEnum(protocol.Cmd.read_eeprom);
        buf[1] = @intCast((address >> 8) & 0xFF);
        buf[2] = @intCast(address & 0xFF);
        _ = self.setFeature(&buf) catch {};
        // Zero out and re-read
        @memset(&buf, 0);
        buf[0] = @intFromEnum(protocol.Cmd.read_eeprom);
        try self.getFeature(&buf);
        return (@as(u16, buf[3]) << 8) | buf[4];
    }

    /// Write EEPROM at given address with 16-bit value.
    /// Maps to HID_EE_Write16 from the original binary.
    pub fn writeEeprom16(self: *DeviceHandle, address: u16, value: u16) Error!void {
        var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
        buf[0] = @intFromEnum(protocol.Cmd.write_eeprom);
        buf[1] = @intCast((address >> 8) & 0xFF);
        buf[2] = @intCast(address & 0xFF);
        buf[3] = @intCast((value >> 8) & 0xFF);
        buf[4] = @intCast(value & 0xFF);
        try self.setFeature(&buf);
    }

    /// Read device status.
    /// Note: Some X7 variants (PID 9066) timeout on setFeature but respond to
    /// getFeature directly. We try setFeature first but don't fail on timeout.
    pub fn readStatus(self: *DeviceHandle) Error!protocol.DeviceStatus {
        var buf: [protocol.report_size]u8 = [_]u8{0} ** protocol.report_size;
        buf[0] = @intFromEnum(protocol.Cmd.read_status);
        _ = self.setFeature(&buf) catch |e| switch (e) {
            error.Timeout => {}, // Some devices timeout on setFeature — that's OK
            error.TransferFailed => {}, // Also OK, device may not need setFeature
            else => |other| return other,
        };
        @memset(&buf, 0);
        buf[0] = @intFromEnum(protocol.Cmd.read_status);
        try self.getFeature(&buf);
        return protocol.DeviceStatus{
            .firmware_version = (@as(u16, buf[1]) << 8) | buf[2],
            .battery_level = if (buf[3] > 0 and buf[3] <= 100) buf[3] else null,
            .current_profile = buf[4],
            .current_dpi_stage = buf[5],
            .report_rate = @enumFromInt((@as(u16, buf[6]) << 8) | buf[7]),
        };
    }
};

/// Enumerate all Oscar-compatible devices.
pub fn enumerate(ctx: *c.libusb_context, allocator: std.mem.Allocator) ![]DeviceHandle {
    var list: [*c]?*c.libusb_device = undefined;
    const count = c.libusb_get_device_list(ctx, &list);
    if (count < 0) return Error.InitFailed;
    defer c.libusb_free_device_list(list, 1);

    var devices = std.ArrayList(DeviceHandle).init(allocator);
    errdefer {
        for (devices.items) |*d| c.libusb_close(d.handle);
        devices.deinit();
    }
    var i: isize = 0;
    while (i < count) : (i += 1) {
        const dev = (list[@as(usize, @intCast(i))]) orelse continue;
        var desc: c.libusb_device_descriptor = undefined;
        const rc = c.libusb_get_device_descriptor(dev, &desc);
        if (rc < 0) continue;

        // Check if this device matches a known Oscar device
        for (protocol.known_devices) |known| {
            if (desc.idVendor == known.vid and desc.idProduct == known.pid) {
                // Found an Oscar device
                var handle: ?*c.libusb_device_handle = null;
                if (c.libusb_open(dev, &handle) < 0) continue;
                if (handle == null) continue;

                var ep_in: u8 = 0;
                var ep_out: u8 = 0;
                var iface: c_int = -1;

                // Find HID interface and endpoints
                var config: ?*c.libusb_config_descriptor = undefined;
                if (c.libusb_get_config_descriptor(dev, 0, &config) == 0) {
                    defer c.libusb_free_config_descriptor(config);
                    if (config) |cfg| {
                        var intf_idx: c_int = 0;
                        while (intf_idx < cfg.bNumInterfaces) : (intf_idx += 1) {
                            const intf = &cfg.interface[@as(usize, @intCast(intf_idx))];
                            var alt_idx: c_int = 0;
                            while (alt_idx < intf.num_altsetting) : (alt_idx += 1) {
                                const alt = &intf.altsetting[@as(usize, @intCast(alt_idx))];
                                if (alt.bInterfaceClass == 3) { // HID class
                                    iface = intf_idx;
                                    var ep_idx: c_int = 0;
                                    while (ep_idx < alt.bNumEndpoints) : (ep_idx += 1) {
                                        const ep = &alt.endpoint[@as(usize, @intCast(ep_idx))];
                                        if ((ep.bmAttributes & 3) == 3) { // Interrupt
                                            if ((ep.bEndpointAddress & 0x80) != 0) {
                                                ep_in = ep.bEndpointAddress;
                                            } else {
                                                ep_out = ep.bEndpointAddress;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (iface < 0) {
                    c.libusb_close(handle.?);
                    continue;
                }

                // Skip kernel driver detach — detach kills the mouse/kbd input path.
                // Only detach if driver is confirmed active.
                const driver_active = c.libusb_kernel_driver_active(handle.?, @intCast(iface));
                if (driver_active == 1) {
                    _ = c.libusb_detach_kernel_driver(handle.?, @intCast(iface));
                }
                if (c.libusb_claim_interface(handle.?, @intCast(iface)) < 0) {
                    // On failure, reattach if we detached
                    if (driver_active == 1) {
                        _ = c.libusb_attach_kernel_driver(handle.?, @intCast(iface));
                    }
                    c.libusb_close(handle.?);
                    continue;
                }

                devices.append(.{
                    .handle = handle.?,
                    .dev = dev,
                    .descriptor = desc,
                    .interface = @intCast(iface),
                    .ep_in = ep_in,
                    .ep_out = ep_out,
                    .chipset = known.chipset,
                }) catch {
                    c.libusb_close(handle.?);
                    continue;
                };
            }
        }
    }

    return devices.toOwnedSlice();
}

/// Close a device handle, release interface, close libusb handle.
pub fn closeDevice(dev: *DeviceHandle) void {
    _ = c.libusb_release_interface(dev.handle, dev.interface);
    // Reattach kernel driver if it was detached on claim
    // LIBUSB_ERROR_NOT_FOUND (-5) = no driver to reattach — expected
    const rc = c.libusb_attach_kernel_driver(dev.handle, @intCast(dev.interface));
    if (rc < 0 and rc != c.LIBUSB_ERROR_NOT_FOUND) {
        std.debug.print("Warning: failed to reattach kernel driver on intf {}: {s}\n", .{
            dev.interface, c.libusb_error_name(rc),
        });
    }
    c.libusb_close(dev.handle);
}
