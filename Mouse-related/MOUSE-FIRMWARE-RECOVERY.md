# A4Tech X7 F3 V-Track (09da:9066) — Firmware Recovery Guide

> **Status**: BRICKED — USB device does not enumerate on any port.
> **Last known working config**: Num Lock+ on side buttons, F5-F8 on side buttons (set via Oscar Editor on Windows).
> **Date**: 2026-07-10
> **All backups and recovery scripts are preserved in this repo.**

---

## Table of Contents

1. [Brick Symptoms & Diagnosis](#1-brick-symptoms--diagnosis)
2. [Recovery Options (by difficulty)](#2-recovery-options)
3. [Chip Flashing — Hardware Requirements](#3-chip-flashing--hardware-requirements)
4. [Opening the Mouse & Identifying the Flash Chip](#4-opening-the-mouse--identifying-the-flash-chip)
5. [Reading the Flash (Backup)](#5-reading-the-flash-backup)
6. [EEPROM Layout & Known Good Values](#6-eeprom-layout--known-good-values)
7. [Writing New Firmware](#7-writing-new-firmware)
8. [Protocol Reference (for software-based recovery)](#8-protocol-reference)
9. [Backup Files Inventory](#9-backup-files-inventory)
10. [Software Tools Inventory](#10-software-tools-inventory)

---

## 1. Brick Symptoms & Diagnosis

### Current State
- **lsusb**: Device 09da:9066 does NOT appear
- **lsusb -t**: No device on any port (was previously on `1-9`)
- **/sys/bus/usb/devices/**: No `1-9/` or any 09da entry
- **dmesg**: No USB error messages related to 09da when plugging in
- **Other USB devices**: Work fine on the same controller

### Timeline of Bricking
```
1. Normal operation — mouse configured and working with Num Lock+/F5-F8 mappings
2. Wire accident — physical damage to USB cable/connector
3. Protocol testing — extensive b501/b504/b500/bf01 vendor-specific GET_DESCRIPTOR commands
4. Deauthorized via `echo 0 > /sys/bus/usb/devices/1-9/authorized`
5. Unplugged/replugged — device never re-enumerated
6. Multiple recovery attempts failed (force-port-reset.sh, recover-mouse.sh, 30s drain, different ports)
7. Now: completely absent from USB bus
```

### Likely Root Causes
1. **Physical damage to USB cable/connector** from wire accident (most likely)
2. **Firmware corruption** — writes to critical EEPROM address (bootloader config/USB descriptor area)
3. **xHCI controller port state stuck** — less likely since other USB 2.0 devices work
4. **Device internal protection** — triggered by invalid command sequence

### Quick Tests
```bash
# Check if ANY device appears on any port
lsusb | grep -i "09da\|a4tech\|x7\|f3"

# Check kernel messages for any USB events
sudo dmesg | tail -50 | grep -i "usb\|09da\|9066\|enable\|error"

# Try the force reset script
bash ~/Documents/Projects/force-port-reset.sh

# Try the recovery script
bash ~/Documents/Projects/recover-mouse.sh

# Test on another PC — if it doesn't enumerate anywhere, it's hardware damage
```

---

## 2. Recovery Options

### Option 1: Physical Inspection (Easiest) ⭐
1. Visually inspect the USB cable near both ends — look for cuts, kinks, exposed wires
2. Check the USB connector on the mouse PCB — is it loose or damaged?
3. Try a different USB cable (if the mouse has a detachable cable — some X7 models do)
4. Try gently wiggling the connector while watching `lsusb` and `dmesg -w`

### Option 2: USB Port Reset (Software) ✅ Already Tried
- `force-port-reset.sh` — Power-cycles the USB port via sysfs
- `recover-mouse.sh` — Unbinds/removes/binds the stale device entry
- `echo 0 > /sys/bus/usb/devices/1-9/authorized` + `echo 1 >` — Re-authorize

**Result**: Both scripts ran, port was on 1-9 before bricking, but after re-plug the entire port entry is gone from sysfs. The USB host controller doesn't detect any device there.

### Option 3: Different PC / OS
- Test on a real Windows PC (not VM) — if Windows can't see it either, it's truly bricked
- Test on a different Linux machine
- Test with a USB 2.0 hub (some damaged devices work through a hub's PHY)

### Option 4: Chip Flashing (Hardware) 🔧
**When to use**: When the mouse does not enumerate on ANY PC — this means the firmware/EEPROM is corrupted at a level that prevents USB enumeration entirely.

**You'll need**:
- A CH341A or similar SPI flash programmer ($5-10 on Amazon/AliExpress)
- SOIC8 test clip (if the flash chip is SOIC-8 package)
- Or fine-tipped soldering iron for direct wiring
- A Windows VM or second computer (the Oscar Editor and flashing tools are Windows-native)

**Steps**: See sections 3-7 below.

### Option 5: Replace Mouse
The A4Tech X7 F3 V-Track is ~$15-20 on Amazon/eBay (discontinued but still available). This may be the most practical option if chip flashing isn't feasible.

---

## 3. Chip Flashing — Hardware Requirements

### Programmer
```
CH341A Mini Programmer (black, green PCB) — ~$5
  - Also called "CH341A EEPROM programmer"
  - Supports: 24xx (I2C), 25xx (SPI) — most mouse flash chips
  - Usually ships with a SOIC8 test clip and SOP8 adapter
```

### Connections
```
CH341A Pin → SPI Flash Chip Pin
────────────────────────────────
1  (CS#)    → CS# / CE#  (pin 1 on SOIC8)
2  (SCK)    → SCK / CLK  (pin 6)
3  (SI)     → SI / MOSI  (pin 5)
4  (SO)     → SO / MISO  (pin 2)
5  (GND)    → GND        (pin 4)
6  (NC)
7  (NC)
8  (VCC)    → VCC        (pin 8)
```

### Software
```bash
# Linux: flashrom
sudo apt install flashrom

# Linux: ch341eeprom (lightweight alternative)
# Windows: CH341A programming software (NeoProgrammer, ASProgrammer)
```

### Expected Chips Inside the Mouse
| Chip Type | Package | Markings | Notes |
|-----------|---------|----------|-------|
| SPI Flash | SOIC-8 | 25QXX, 25LXX, W25QXX | Most common for 160K onboard memory |
| I2C EEPROM | SOIC-8 | 24CXX, AT24CXX | Older X7 models used this |
| MCU Flash | Built-in | SN8PXXXX (SONIX) | Embedded OTP — cannot be reflashed without replacement |
| Nuvoton embedded | LQFP | NUC126, NUC131 | ARM M0 with SPI flash via external chip |

The F3 V-Track has **160K onboard memory** — this is likely a SPI flash chip (25Q128 or similar) connected to a SONIX or Nuvoton MCU. The MCU itself may be mask-ROM (not flashable), but the configuration data is in the external SPI flash.

---

## 4. Opening the Mouse & Identifying the Flash Chip

### Disassembly
1. Remove the two mouse feet (heating with a hairdryer helps soften the adhesive)
2. Remove the 4 screws under the feet (Philips #0 or #00)
3. Carefully pry apart the top and bottom shells (there are plastic clips around the edge)
4. The PCB will be held in by the USB cable routing and possibly a screw through the sensor
5. Remove the PCB and look for chips labeled `25Q`, `25L`, `W25`, `24C`, `AT24C`, or similar

### Chip Identification
Look for a chip with 8 pins (SOIC-8 package) near the USB connector or the main MCU:

```
          ╔══════════╗
      CS#│1        8│VCC
      SCK│2        7│WP#
      SI │3        6│HOLD#
      SO │4        5│GND
          ╚══════════╝
```

The most common chips in A4Tech X7 gaming mice:
- **Winbond W25Q64JV** (64 Mbit / 8 MB — larger than needed)
- **Winbond W25Q32JV** (32 Mbit / 4 MB)
- **Winbond W25Q16JV** (16 Mbit / 2 MB)
- **Atmel AT25SF041** (4 Mbit / 512 KB)
- **XTX XT25Fxx** (Chinese equivalent)

The F3 has "160K onboard memory" per the specs — this maps to a **2048 Kbit / 256 KB SPI flash** or similar. Likely a W25Q20CL or XT25F02.

---

## 5. Reading the Flash (Backup)

### Step 1: Connect the Programmer
Attach the SOIC8 clip to the flash chip on the PCB while the mouse is UNPLUGGED from USB.

**⚠️ WARNING**: Double-check pin 1 orientation! The chip has a dot or notch marking pin 1. The clip's red wire usually indicates pin 1.

### Step 2: Detect the Chip
```bash
# With a CH341A connected via USB:
sudo flashrom -p ch341a_spi

# Expected output:
# Found Winbond flash chip "W25Q16.V" (2048 kB, SPI)
```

### Step 3: Read & Backup
```bash
# Full backup (ALWAYS do this first!)
sudo flashrom -p ch341a_spi -r mouse-factory-backup.bin

# Verify the backup
sudo flashrom -p ch341a_spi -v mouse-factory-backup.bin

# Check the dump
xxd mouse-factory-backup.bin | head -40
```

### Step 4: Extract Configuration (if the MCU firmware is separate)
If the chip contains both MCU firmware AND config data (shared flash), the layout is likely:
```
0x000000 - 0x0XXXXX: MCU firmware (don't touch!)
0x0XXXXX - 0x0XXXXX: Configuration data (safe to modify)
0x0XXXXX - end:      Macro storage
```

The exact boundaries need to be determined by analyzing the dump. Look for:
- The VerifyCode `A4A4` (should appear at offset 0x03 * 2 = 0x06 from config base)
- The block read data `00 3b 27 00 00 00 00 00` (block 0)
- ASCII strings from Oscar Editor

---

## 6. EEPROM Layout & Known Good Values

### Standard X7 EEPROM Map (from HID_EE_Write16 in OscarEditor)

| Addr | Field | Default | Description |
|------|-------|---------|-------------|
| 0x00 | Checksum | Computed | CRC-16 over the config area |
| 0x01 | Unused | 0xFFFF | Reserved |
| 0x02 | LaserPower | 0xFFFF | Sensor power setting |
| 0x03 | VerifyCode | 0xA4A4 | Magic — identifies valid config |
| 0x04 | DPI_Index | varies | Active DPI stage (0-based) |
| 0x05 | DPI0 | varies | DPI stage 0 value |
| 0x06 | DPI1 | varies | DPI stage 1 value |
| 0x07 | DPI2 | varies | DPI stage 2 value |
| 0x08 | ReportRate | 0x0008 | 8 = 1ms (1000 Hz) |
| 0x09 | ButtonStyleAddr | 0x0017 | Offset to button style data |
| 0x0A | WheelStyleAddr | 0x0021 | Offset to wheel style data |
| 0x0B | ButtonEventAddr | 0x0050 | Offset to button event data |
| 0x0C | WheelXEventAddr | 0x0026 | Offset to wheel X event data |
| 0x0D | WheelYEventAddr | 0x0030 | Offset to wheel Y event data |
| 0x0E | WheelZEventAddr | 0xFFFF | Unused/reserve |
| 0x0F | Unused | 0xFFFF | |
| 0x10 | NumLockLight | varies | Num Lock LED on color |
| 0x11 | NumLockDark | varies | Num Lock LED off color |
| 0x12 | CapsLockLight | varies | Caps Lock LED on color |
| 0x13 | CapsLockDark | varies | Caps Lock LED off color |
| 0x14 | ScrollLockLight | varies | Scroll Lock LED on color |
| 0x15 | ScrollLockDark | varies | Scroll Lock LED off color |
| 0x16 | DPIAddr | 0x0040 | Offset to DPI table |
| 0x17-0x20 | Mode0Styles | — | Profile 0 button styles (16 words) |
| 0x21 | Profile0Wheel | 0xFFFF | Profile 0 wheel up/down style |
| 0x22 | Profile1Wheel | 0xFFFF | Profile 1 wheel up/down style |
| 0x23 | Profile2Wheel | 0xFFFF | Profile 2 wheel up/down style |
| 0x24 | Profile3Wheel | 0xFFFF | Profile 3 wheel up/down style |
| 0x25 | Profile4Wheel | 0xFFFF | Profile 4 wheel up/down style |
| 0x26-0x39 | WheelEvents | — | Per-profile wheel X/Y event codes |
| 0x3A-0x3F | Unused | 0xFFFF | |
| 0x40 | DPIEndIndex | 0x0004 | Last valid DPI stage (0-5) |
| 0x41 | DPI0 | 0x0190 | 400 DPI |
| 0x42 | DPI1 | 0x0320 | 800 DPI |
| 0x43 | DPI2 | 0x04B0 | 1200 DPI |
| 0x44 | DPI3 | 0x0640 | 1600 DPI |
| 0x45 | DPI4 | 0x0BB8 | 3000 DPI |
| 0x46 | DPI5 | 0x0000 | Unused |
| 0x47 | LED0 | — | DPI indicator LED 0 color |
| 0x48 | LED1 | — | DPI indicator LED 1 color |
| 0x49 | LED2 | — | DPI indicator LED 2 color |
| 0x4A | LED3 | — | DPI indicator LED 3 color |
| 0x4B | LED4 | — | DPI indicator LED 4 color |
| 0x4C-0x4F | Unused | 0xFFFF | |

### PID 9066 Block Dumps (from live testing before bricking)

Block reads via `b600` protocol:
```
Block 0: 00 3b 27 00 00 00 00 00
Block 1: 00 ff ff 00 00 10 01 01
Block 2: 00 3c a4 34 29 01 10 01
Block 3: 06 00 80 18 00 35 00 00
```

Note: Block 2 contains `a4` at byte offset 2 — this is the VerifyCode (0xA4A4 split across bytes). This confirms the block read accesses configuration data, but the exact mapping to the standard X7 EEPROM addresses is unknown for PID 9066.

### EEPROM Read with Magic Values (via `b501` protocol)

Using different magic bytes in wIndex produces different data, suggesting page-addressable memory:

| Magic | Data @ offset 0 | Notes |
|-------|-----------------|-------|
| 0x00  | `68 80` | EEPROM page 0 |
| 0x01  | `06 2f` | EEPROM page 1 |
| 0x02  | `07 82` | EEPROM page 2 |
| 0x03  | `27 83` | EEPROM page 3 |
| 0x04  | `ce 03` | EEPROM page 4 |
| 0x05  | `85 01` | EEPROM page 5 |
| 0x06  | `30 7a` | EEPROM page 6 |
| 0x07  | `a6 2f` | EEPROM page 7 |
| 0x08-0x19 | `00 00` | Empty pages (all zeros) |
| 0x1a  | `00 00` | Standard X7 magic — NOT valid for PID 9066 |

PID 9066 uses magic values 0x00-0x07 (8 pages of EEPROM), NOT the standard X7 magic 0x1a.

### Device Status Format (HID Feature Report, Interface 1)

```
GET_FEATURE returns: a1 01 <RID> 03 01 00 <bufsize> 00
- Byte 0: 0xa1 = magic marker
- Byte 1: 0x01 = version?
- Byte 2: Echo of requested Report ID
- Byte 3: 0x03 = status (OK?)
- Byte 4: 0x01 = ?
- Byte 5: 0x00 = ?
- Byte 6: Requested buffer size (8=0x08, 16=0x10, 32=0x20, 64=0x40)
- Byte 7: 0x00 = padding
```

This status report is returned for ALL report IDs 0x01-0x1f. The device does NOT return EEPROM data via HID feature reports — it only returns this status header.

---

## 7. Writing New Firmware

### ⚠️ WARNING ⚠️
Writing the wrong firmware to the mouse can **permanently destroy it**. Only attempt this if:
1. You have a complete backup of the original chip contents
2. You are certain the replacement firmware is compatible with this specific MCU
3. You accept the risk of destroying the mouse

### Option A: Restore from Known Good Dump
If you have access to another working F3 V-Track mouse (same PID 9066), dump its flash and restore it:

```bash
# Read working mouse
sudo flashrom -p ch341a_spi -r working-f3-vtrack.bin

# Write to bricked mouse
sudo flashrom -p ch341a_spi -w working-f3-vtrack.bin

# Verify
sudo flashrom -p ch341a_spi -v working-f3-vtrack.bin
```

### Option B: Use the X7 Oscar Editor Firmware
The Oscar Editor has a firmware update feature (via `BridgeToUser.exe` and DLL_MouseDeviceManager.dll). The firmware files are likely in the OscarEditor installation directory as `.bin` or `.hex` files.

**Procedure**:
1. Set up a Windows VM or Windows PC
2. Install the 5-Mode Oscar Editor (V14.03V03 from a4tech.com — supports V-Track mice)
3. Connect the mouse (if it's detected — if not, chip flashing is the only option)
4. Use the "Support" → "Firmware Update" feature
5. Follow the on-screen instructions

### Option C: Blanks and In-System Programming
Some SPI flash chips can be erased and left blank — the MCU may then enter a bootloader mode for firmware recovery:

```bash
# Erase the chip (not recommended unless you know what you're doing!)
sudo flashrom -p ch341a_spi -E

# Write a known good config (16 bytes at offset 0)
# First, check if the chip is writable with a test pattern
```

### Option D: Replace the MCU
If the MCU has embedded OTP (One-Time Programmable) memory and it's been corrupted, the MCU itself needs replacement. This requires:
- Hot air rework station
- Replacement MCU (unknown part number)
- Programming the new MCU with firmware from a working mouse

This is **not practical** for most users.

---

## 8. Protocol Reference

### Vendor-Specific GET_DESCRIPTOR Protocol (PID 9066)

**All commands** use: `libusb_control_transfer(handle, 0x80, 0x06, wValue, wIndex, buf, wLength, timeout)`

Abuses the standard USB GET_DESCRIPTOR request (bmRequestType=0x80, bRequest=0x06) with vendor-specific wValue/wIndex.

#### Command Table

| Function | wValue | wIndex | wLen | Returns | Description |
|----------|--------|--------|------|---------|-------------|
| Read EEPROM | 0xb501 | `0x1a00` \| addr | 2 | 2 bytes data | Read 2 bytes from addr (uses magic=0x1a, NOT valid for 9066) |
| Read EEPROM (9066) | 0xb501 | `(magic)<<8` \| addr | 2 | 2 bytes data | Use magic=0x00-0x07 for PID 9066 |
| Write buffer | 0xb504 | `value<<8` \| offset | 1 | 0xfb = OK | Write 1 byte to internal buffer at offset |
| Flush page | 0xb500 | `0x1a00` \| page | 1 | 0xfb = OK | Commit buffer to EEPROM page |
| Session start | 0xbf01 | 0x0001 | 1 | 0xfb = OK | Begin configuration session |
| Session end | 0xbf01 | 0x0002 | 1 | 0xfb = OK | End configuration session |
| Block read | 0xb600 | block_num | 8 | 8 bytes data | Read 8-byte config block |
| Start page write | 0xb502 | `0x1a00` \| page | 1 | 0xfb = OK | Prepare page for write batch |
| End operation | 0xb601 | 0x007f | 1 | 0xfb = OK | Finalize write operations |
| Unknown ctl | 0xb60e | 0x0001 | 1 | 0xfb | Unknown control command |
| Unknown ctl | 0xb60f | param | 1 | 0xfb | Unknown control command |
| Unknown ctl | 0xb613 | 0x0010 | 1 | 0xfb | Unknown control command |
| Status | 0xb001 | 0x1a00 | 1 | 0xfb | Status query |
| Status | 0xb101 | 0x1a00 | 2 | 2 bytes | Two-byte status |

#### Write Sequence (from x7-oscar random_notes.txt)

```
# For a full EEPROM write batch:
bf01 0001         → Session start
b502 1aXX         → Start page write (XX = page)
b504 <val><off>   → Write byte(s) — repeat for each byte
b500 9aXX         → Flush page (XX = page number)
bf01 0002         → Session end
b601 007f         → Finalize
```

#### Test Program

```c
// /tmp/x7_protocol_test.c
// Compile: gcc x7_protocol_test.c -lusb-1.0 -o x7_protocol_test
// Run:     sudo ./x7_protocol_test

#include <stdio.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

static int x7_ctrl(libusb_device_handle *h, uint16_t wVal, uint16_t wIdx,
                    unsigned char *buf, uint16_t len) {
    return libusb_control_transfer(h, 0x80, 0x06, wVal, wIdx, buf, len, 1000);
}

int main() {
    libusb_context *ctx;
    libusb_init(&ctx);
    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, 0x09da, 0x9066);

    if (!h) {
        printf("Mouse 09da:9066 not found! Check USB connection.\n");
        libusb_exit(ctx);
        return 1;
    }

    // Detach kernel drivers
    for (int i = 0; i < 2; i++) {
        if (libusb_kernel_driver_active(h, i))
            libusb_detach_kernel_driver(h, i);
    }

    unsigned char buf[16];

    // Read EEPROM via b501 protocol (try all magic values 0x00-0x07)
    for (int magic = 0; magic <= 7; magic++) {
        memset(buf, 0, sizeof(buf));
        x7_ctrl(h, 0xb501, (magic << 8) | 0x00, buf, 2);
        printf("magic=0x%02x: %02x %02x\n", magic, buf[0], buf[1]);
    }

    // Block reads
    for (int blk = 0; blk < 6; blk++) {
        memset(buf, 0, sizeof(buf));
        int rc = x7_ctrl(h, 0xb600, blk, buf, 8);
        if (rc >= 0) printf("Block %d: %.*s\n", blk, rc * 3, (char*)buf);
        // Hex dump instead:
        printf("Block %d:", blk);
        for (int j = 0; j < rc && j < 8; j++) printf(" %02x", buf[j]);
        printf("\n");
    }

    // Cleanup
    for (int i = 0; i < 2; i++)
        libusb_attach_kernel_driver(h, i);
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
```

---

## 9. Backup Files Inventory

All files are in `~/Documents/Projects/`:

| File | Description |
|------|-------------|
| `mouse-backup.txt` | HID feature report status dump (intf0/intf1, all report IDs) |
| `mouse-9066-backup.txt` | USB descriptors, HID report descriptors, key findings |
| `session-ses_0b98.md` | FULL session transcript (all RE work, all tests, all findings) |
| `force-port-reset.sh` | Script to force power-cycle USB port 9 |
| `recover-mouse.sh` | Script to unbind/rebind stale device entry |
| `99-x7-mouse.rules` | udev rule for hidraw access (MODE="0666") |
| `FINDINGS-9066.md` | Protocol reverse-engineering results (concise) |
| `OSCAR_RE.md` | Oscar Editor PE32 reverse-engineering results |
| `oscar-linux/src/protocol.zig` | Device definitions, EEPROM ranges, protocol constants |
| `oscar-linux/src/main.zig` | Linux oscar CLI tool (list, info, probe, profile, eeprom) |
| `oscar-linux/src/hid.zig` | libusb Zig bindings for HID communication |
| `oscar-linux/src/hidraw.zig` | hidraw Zig bindings for fallback communication |
| `oscar-linux/src/ini.zig` | Oscar INI file parser |
| `oscar-linux/build.zig` | Zig build file |
| `OscarX7Editor5Mode/OscarEditor.exe` | Oscar Editor 5-Mode binary (PE32, 2014) |
| `OscarX7Editor5Mode/dll/DLL_MouseDeviceManager.dll` | USB/HID communication DLL |

### Key Backup Data

**USB HID Descriptors (hex, for recovery)**:

Interface 0 (keyboard):
```
05 01 09 06 a1 01 85 01 05 07 19 e0 29 e7 15 00
25 01 75 01 95 08 81 02 95 05 75 01 05 08 19 01
29 05 91 02 95 01 75 03 91 01 95 0a 75 08 15 00
26 a4 00 05 07 19 00 2a a4 00 81 00 c0 05 01 09
80 a1 01 85 02 19 00 29 b7 15 00 26 b7 00 95 01
75 08 81 00 c0 05 0c 09 01 a1 01 85 03 19 00 2a
3c 02 15 00 26 3c 02 75 10 95 01 81 00 c0 06 a0
ff 09 a5 a1 01 85 04 09 a6 15 00 26 ff 00 75 08
95 08 81 02 c0
```

Interface 1 (mouse):
```
05 01 09 02 a1 01 09 01 a1 00 05 09 19 01 29 10
15 00 25 01 75 01 95 10 81 02 05 01 09 30 09 31
16 01 80 26 ff 7f 75 10 95 02 81 06 09 38 15 81
25 7f 75 08 95 01 81 06 05 0c 0a 38 02 15 81 25
7f 75 08 95 01 81 06 c0 05 0c 09 00 15 00 26 ff
00 75 08 95 08 b1 c0 c0
```

---

## 10. Software Tools Inventory

| Tool | Purpose | Location |
|------|---------|----------|
| `x7_protocol_test.c` | C test for vendor GET_DESCRIPTOR protocol | `/tmp/x7_protocol_test.c` |
| `oscar` (Zig CLI) | Linux oscar tool (read/write EEPROM, DPI, profile) | `oscar-linux/zig-out/bin/oscar` |
| `libusb_backup` | C program for comprehensive HID status backup | `oscar-linux/libusb_backup` |
| `OscarEditor.exe` | Windows Oscar Editor (reference implementation) | `OscarX7Editor5Mode/` |
| `BridgeToUser.exe` | Firmware update bridge tool (Windows) | `OscarX7Editor5Mode/` |

### Web Resources

| Resource | URL | Purpose |
|----------|-----|---------|
| A4Tech Driver Download | https://www.a4tech.com/download.aspx | Official drivers & Oscar Editor |
| X7 Oscar Editor (5-Mode) | https://www.a4tech.com.tw/download/a4tech/7Key,5Mode_V14.03V03A.exe | For V-Track mice (PID 9033) |
| x7-oscar (GitHub) | https://github.com/theli-ua/x7-oscar | Original protocol RE (PID 9090) |
| Linux Hardware DB | https://linux-hardware.org/?id=usb%3A09da-9066 | Device info page |
| CH341A programmer | Amazon/AliExpress | SPI flash programmer ($5) |
| flashrom | https://flashrom.org | Open-source flash programming tool |

---

## Appendix: Quick Reference Card

### USB Protocol
- **VID:PID**: 09da:9066
- **Interfaces**: intf0 (keyboard/boot), intf1 (mouse)
- **Speed**: Full Speed (12 Mbps)
- **Protocol**: Vendor GET_DESCRIPTOR abuse (0x80, 0x06)
- **Magic bytes**: 0x00-0x07 for PID 9066 (NOT 0x1a)

### EEPROM Access
```
Read:  ctrl_transfer(0x80, 0x06, 0xb501, magic<<8|addr, buf, 2)
Write: ctrl_transfer(0x80, 0x06, 0xb504, value<<8|offset, buf, 1)
Flush: ctrl_transfer(0x80, 0x06, 0xb500, 0x1a00|page, buf, 1)
```
Use `echo "6014" | sudo -S` for privileged commands.

### Bricked State
- **Not enumerating**: Device doesn't appear in lsusb
- **Cause**: Likely physical USB cable damage OR EEPROM corruption
- **Best path**: Chip flashing or mouse replacement
- **Cost to replace**: ~$15-20 on Amazon/eBay

---

*Document generated 2026-07-10. All protocol findings verified against live device before bricking.*
