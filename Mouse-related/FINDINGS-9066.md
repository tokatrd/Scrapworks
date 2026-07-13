# A4Tech X7 F3 V-Track (09da:9066) Protocol Reverse-Engineering Findings

## Device Properties
- **VID:PID**: 09da:9066
- **Name**: A4Tech F3 V-Track Gaming Mouse  
- **USB Class**: HID (Interface 0: Keyboard/boot, Interface 1: Mouse)
- **USB Speed**: Full Speed (12 Mbps)
- **Chipset**: X7-family variant (SONIX or similar)

## Communication Protocol

### 1. Standard HID Feature Reports (HidD_GetFeature/SetFeature)
- **GET_REPORT** (Feature): Returns 8-byte status report regardless of Report ID
  - Format: `a1 01 <RID> 03 01 00 <len> 00`
  - Always returns status, never EEPROM data
- **SET_REPORT** (Feature/Output): Works when kernel driver is detached via libusb
  - Returns success but effect is unclear
- **Conclusion**: Standard HID feature reports DO NOT provide EEPROM access on PID 9066

### 2. Vendor-Specific GET_DESCRIPTOR Protocol (Real X7 Protocol)
**This is the correct protocol for EEPROM access.**

Uses standard USB `GET_DESCRIPTOR` (bRequest=0x06, bmRequestType=0x80) with vendor-specific descriptor types.

#### EEPROM Word Read: `b501`
```
ctrl_transfer(0x80, 0x06, 0xb501, magic<<8 | addr, buf, 2, 500)
```
- **wValue = 0xb501** (descriptor type 0xb5, index 0x01 = read command)
- **wIndex = {magic_byte} {addr}** (magic selects memory page, addr is offset within page)
- **wLength = 2** (returns 2 bytes)

**KEY DISCOVERY**: Standard X7 uses magic byte `0x1a` (wIndex format `0x1a00 | addr`), which returns `00 00` for all addresses on PID 9066. However, **different magic values return different data**:

| Magic | Data @ addr 0x00 | Notes |
|-------|-------------------|-------|
| 0x00  | `68 80` | EEPROM page 0 |
| 0x01  | `06 2f` | EEPROM page 1 |
| 0x02  | `07 82` | EEPROM page 2 |
| 0x03  | `27 83` | EEPROM page 3 |
| 0x04  | `ce 03` | EEPROM page 4 |
| 0x05  | `85 01` | EEPROM page 5 |
| 0x06  | `30 7a` | EEPROM page 6 |
| 0x07  | `a6 2f` | EEPROM page 7 |
| 0x1a  | `00 00` | Standard X7 magic — NOT valid for PID 9066 |

#### EEPROM Byte Write: `b504`
```
ctrl_transfer(0x80, 0x06, 0xb504, value<<8 | offset, buf, 1, 500)
```
- **wValue = 0xb504** (descriptor type 0xb5, index 0x04 = write command)
- **wIndex = {value} {offset}** (value=byte to write, offset=position in buffer)
- Returns 1 byte: `0xfb` = success

#### EEPROM Flush/Commit: `b500`
```
ctrl_transfer(0x80, 0x06, 0xb500, 0x1a00 | page, buf, 1, 500)
```
- **wValue = 0xb500** (descriptor type 0xb5, index 0x00 = flush command)
- **wIndex = 0x1aXX** (low byte = page number? number of bytes?)
- Returns 1 byte: `0xfb` = success

**NOTE**: The write+flush sequence returns success but persistence to block reads was not verified before the device firmware crashed. The block read DID change once during testing (byte 0 of block 1 went from `0x7f` to `0x00`), suggesting writes work but the mapping is not yet understood.

#### Session Control: `bf01`
```
ctrl_transfer(0x80, 0x06, 0xbf01, session_cmd, buf, 1, 500)
```
- session_cmd = 0x0001: Session start
- session_cmd = 0x0002: Session end
- Returns 1 byte: `0xfb` = success

#### Status/Feature Reads: `b5XX`
| wValue  | wIndex   | Result | Notes |
|---------|----------|--------|-------|
| 0xb500  | 0x1aXX   | `fb`   | Flush/commit command |
| 0xb501  | 0x1aXX   | `00 00` | Standard X7 EEPROM read (not valid) |
| 0xb502  | 0x1a00   | `fb`   | Unknown command |
| 0xb503  | 0x1a00   | PIPE   | Invalid command |
| 0xb504  | 0x1a00   | `fb`   | Write buffer command |
| 0xb505  | 0x1a00   | `1a a4` | Unknown — reads with magic prefix? |
| 0xb506-0xb50f | 0x1a00 | `fb` | Various commands |

### 3. Block Read Protocol: `b600`
```
ctrl_transfer(0x80, 0x06, 0xb600, block_num, buf, 8, 500)
```
- Returns exactly 8 bytes of config block data
- wLength < 8 → OVERFLOW error
- wLength > 8 → returns exactly 8 bytes (rc=8)

**Block Dump (PID 9066)**:
| Block | Data |
|-------|------|
| 0     | `00 3b 27 00 00 00 00 00` |
| 1     | `00 ff ff 00 00 10 01 01` |
| 2     | `00 3c a4 34 29 01 10 01` |
| 3     | `06 00 80 18 00 35 00 00` |
| 4     | `00 3b 27 00 00 00 00 00` (same as block 0 — wrap-around) |
| 5+    | Echoes USB setup packet header + garbage — beyond config memory |

**Note**: Block 2 contains byte `0xa4` which matches the X7 VerifyCode. This suggests the config blocks overlap with the standard X7 EEPROM map but use a different access method.

### 4. Other Descriptor Types Discovered
| wValue  | wIndex   | Result | Notes |
|---------|----------|--------|-------|
| 0xbf01  | 0x0001   | `fb`   | Session start |
| 0xbf01  | 0x0002   | `fb`   | Session end |
| 0xb001  | 0x1a00   | `fb`   | Status query? |
| 0xb101  | 0x1a00   | `01 00` | Two-byte status |
| 0xb201  | 0x1a00   | `fb`   | Another status/command |
| 0xb301  | 0x1a00   | `fb`   | Another status/command |

### 5. Write Status via Libusb (kernel driver detached)
- **SET_REPORT** (Feature): Always returns success (no data returned)
- **SET_REPORT** (Output): Always returns success
- **Interface 0** (keyboard): PIPE error on GET_REPORT
- **Interface 1** (mouse): Status report on GET_REPORT

## Write Buffer / Page Model (Hypothesis)
The EEPROM write uses a buffer+flush model:
1. Write bytes to internal buffer using `b504` (each call writes 1 byte at an offset)
2. Commit buffer to EEPROM page using `b500 1a{page}`
3. Read back using `b501` with appropriate magic

**Unknowns for PID 9066**:
- Correct magic byte for writes (standard X7 uses 0x1a, PID 9066 uses 0x00-0x07 for reads)
- Whether the `b500` low byte is a page number or byte count
- Whether write buffer offset maps to EEPROM address directly or within a page
- Whether session (bf01) is required for writes

## Device Firmware Crash Analysis
During testing, the device firmware entered an unrecoverable state:
1. Multiple `b501` EEPROM reads with various magic values
2. `b504` writes + `b500` flushes at various offsets/pages
3. `bf01` session start/end commands
4. `echo 0 > /sys/bus/usb/devices/1-9/authorized` deauthorized the device
5. After unplug/replug, the device won't enumerate — USB controller reports `-71 (EPROTO)`

**Possible causes**:
- Write to a critical EEPROM address (bootloader config, USB descriptor area)
- xHCI controller port in bad state (less likely since other USB 2.0 devices on the bus work)
- The device has an internal protection mechanism that was triggered

## Recovery Attempts
- `echo 1 > /sys/bus/usb/devices/1-9/authorized` — reauthorized but device unresponsive
- Unplug/replug after 2+ minutes — device not detected on any USB bus
- Recovery script at `/tmp/recover-usb.sh`: removes stale sysfs entry, forces re-detection

## Files
- `oscar-linux/src/protocol.zig` — Device definitions and protocol constants
- `mouse-9066-backup.txt` — Full USB descriptor dump and protocol test results
- `mouse-backup.txt` — HID feature report status backup
