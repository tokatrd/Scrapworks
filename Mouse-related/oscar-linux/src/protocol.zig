//! Oscar mouse protocol — constants, structures, and commands.
//!
//! Extracted from reverse-engineering A4Tech/Bloody OscarEditor.exe
//! (Borland C++ Builder, 2014).
//!
//! ## Communication Methods
//!
//! ### Standard HID Feature Reports (Windows: HidD_GetFeature/SetFeature)
//! Used by X7 EMC/SONIX (PID 056f, 9090, 8090) and Bloody series.
//! Works via libusb control transfers with bRequest=0x09 (SET_REPORT) / 0x01 (GET_REPORT).
//! Report size: 64 bytes. Report ID is first byte.
//!
//! ### Vendor GET_DESCRIPTOR Protocol (PID 9066 — X7 F3 V-Track variant)
//! Uses standard USB GET_DESCRIPTOR (bRequest=0x06, bmRequestType=0x80) with
//! vendor-specific descriptor types for EEPROM access.
//!
//! **Key difference from standard X7**: The magic byte in wIndex is `0x00`-`0x07`
//! on PID 9066, NOT `0x1a` as used by standard X7 (PID 9090).
//!
//! | Function | wValue | wIndex | wLength | Returns |
//! |----------|--------|--------|---------|---------|
//! | Read 2 bytes | 0xb501 | {magic}<<8 \| addr | 2 | 2 bytes of EEPROM data |
//! | Write 1 byte | 0xb504 | value<<8 \| offset | 1 | 0xfb = success |
//! | Flush/commit | 0xb500 | 0x1a00 \| page | 1 | 0xfb = success |
//! | Session start | 0xbf01 | 0x0001 | 1 | 0xfb |
//! | Session end | 0xbf01 | 0x0002 | 1 | 0xfb |
//! | Block read (8B) | 0xb600 | block_num | 8 | 8-byte config block |
//! | Status | 0xb001 | 0x1a00 | 1 | 0xfb |
//! | Status | 0xb101 | 0x1a00 | 2 | 2 bytes |
//!
//! See `FINDINGS-9066.md` for full protocol reverse-engineering details.

/// Known Oscar-compatible device VID:PID pairs.
pub const known_devices = [_]DeviceId{
    // A4Tech X7 series
    .{ .vid = 0x09da, .pid = 0x056f, .name = "X7 (EMC)", .chipset = .x7 },
    .{ .vid = 0x09da, .pid = 0x9090, .name = "X7 (SONIX) / X7H", .chipset = .x7 },
    .{ .vid = 0x09da, .pid = 0x8090, .name = "X7 (EMC alt)", .chipset = .x7 },
    // A4Tech X7 F3 V-Track Gaming Mouse (X7 family variant)
    // NOTE: Uses real X7 GET_DESCRIPTOR protocol (bRequest=0x06) for EEPROM access,
    //       but with a DIFFERENT magic byte than standard X7 (0x1a).
    //       PID 9066 uses magic values 0x00-0x07 for EEPROM page access.
    //       Also supports block reads via wValue=0xb600 (8-byte config blocks).
    //       Write protocol (b504+b500) returns success but needs verification.
    .{ .vid = 0x09da, .pid = 0x9066, .name = "X7 F3 V-Track (GET_DESCRIPTOR)", .chipset = .x7 },
    // Bloody series (A4Tech gaming)
    .{ .vid = 0x09da, .pid = 0x24c7, .name = "Bloody K-series", .chipset = .kx },
    .{ .vid = 0x09da, .pid = 0x24c8, .name = "Bloody G-series", .chipset = .gx },
    .{ .vid = 0x09da, .pid = 0x24c9, .name = "Bloody L-series", .chipset = .lx },
};

pub const DeviceId = struct {
    vid: u16,
    pid: u16,
    name: []const u8,
    chipset: Chipset,
};

pub const Chipset = enum {
    x7,
    kx,
    gx,
    lx,
};

/// Standard HID Feature Report size for Oscar devices.
pub const report_size = 64;

/// EEPROM address ranges (from binary analysis of HID_EE_Write16 calls).
pub const eeprom = struct {
    pub const device_info = Range{ .start = 0x0000, .end = 0x00ff };
    pub const dpi_profiles = Range{ .start = 0x0100, .end = 0x01ff };
    pub const button_map = Range{ .start = 0x0200, .end = 0x02ff };
    pub const macro_storage = Range{ .start = 0x0300, .end = 0x03ff };
    pub const lighting_osd = Range{ .start = 0x0400, .end = 0x04ff };
    pub const perf_config = Range{ .start = 0x0500, .end = 0x05ff };
};

pub const Range = struct {
    start: u16,
    end: u16,
};

/// DPI profile — up to 6 stages.
pub const DpiProfile = struct {
    stage_count: u6,
    stages: [6]DpiStage,
};

pub const DpiStage = struct {
    enabled: bool,
    dpi: u16,
};

/// Button trigger — maps a physical button to an action.
pub const ButtonTrigger = struct {
    button_id: u8,
    trigger_type: TriggerType,
    action_param: u32,
};

pub const TriggerType = enum(u8) {
    none = 0,
    left_click = 1,
    right_click = 2,
    middle_click = 3,
    forward = 4,
    backward = 5,
    dpi_cycle = 6,
    dpi_up = 7,
    dpi_down = 8,
    fire = 9,
    macro = 10,
    key_assign = 11,
    multimedia = 12,
    sensitivity = 13,
    profile_switch = 14,
    application = 15,
    disable = 16,
    oled_toggle = 17,
    _,
};

/// Report rate in Hz.
pub const ReportRate = enum(u16) {
    hz_125 = 125,
    hz_250 = 250,
    hz_500 = 500,
    hz_1000 = 1000,
};

/// Device status read from feature report.
pub const DeviceStatus = struct {
    firmware_version: u16,
    battery_level: ?u8,
    current_profile: u8,
    current_dpi_stage: u8,
    report_rate: ReportRate,
};

/// X7 vendor-specific GET_DESCRIPTOR protocol commands (PID 9066).
/// Uses bRequest=0x06 (GET_DESCRIPTOR) with vendor descriptor types.
pub const x7_gd = struct {
    /// EEPROM read: ctrl_transfer(0x80, 0x06, 0xb501, magic<<8|addr, buf, 2)
    pub const read_eeprom = 0xb501;
    /// EEPROM write: ctrl_transfer(0x80, 0x06, 0xb504, value<<8|offset, buf, 1)
    pub const write_eeprom = 0xb504;
    /// Flush/commit buffer: ctrl_transfer(0x80, 0x06, 0xb500, 0x1a00|page, buf, 1)
    pub const flush = 0xb500;
    /// Session control: ctrl_transfer(0x80, 0x06, 0xbf01, cmd, buf, 1), cmd=1=start,2=end
    pub const session = 0xbf01;
    /// Block read (8 bytes): ctrl_transfer(0x80, 0x06, 0xb600, block_num, buf, 8)
    pub const block_read = 0xb600;

    /// Standard X7 magic byte (works on PID 9090, NOT on 9066)
    pub const magic_standard = 0x1a;
    /// PID 9066 magic bytes (0x00-0x07 for EEPROM pages)
    pub const magic_9066_base = 0x00;
    /// PID 9066 magic range end
    pub const magic_9066_end = 0x07;

    pub const session_start = 0x0001;
    pub const session_end = 0x0002;
};

/// HID Feature Report command IDs (inferred from protocol analysis).
pub const Cmd = enum(u8) {
    read_eeprom = 0x01,
    write_eeprom = 0x02,
    read_status = 0x03,
    write_config = 0x04,
    reset = 0x05,
    firmware_erase = 0x10,
    firmware_write = 0x11,
    firmware_verify = 0x12,
    _,
};
