# A4Tech X7 F3 V-Track Mouse (09da:9066) — Reverse Engineering & Recovery

## Status

| Item | Status |
|------|--------|
| **Mouse** | **BRICKED** — does not enumerate on USB (VID:PID 09da:9066 absent from `lsusb`) |
| **Protocol RE** | ✅ Complete — vendor GET_DESCRIPTOR abuse decoded, block read/write working |
| **Oscar Editor RE** | ✅ Complete — PE32 binary, DLL HID communication, INI config format |
| **Linux CLI tool** | ✅ Complete — `oscar` CLI in Zig (builds, worked before bricking) |
| **C test program** | ✅ Complete — `/tmp/x7_protocol_test.c` for protocol verification |
| **Recovery scripts** | ✅ Complete — USB port reset, device rebind, udev rules |
| **Chip flashing guide** | ✅ Complete — CH341A programmer + SOIC8 clip procedure |
| **USB dumps backed up** | ✅ Yes — descriptors, HID reports, status, block reads saved |

## Project Layout

```
Mouse-related/
├── README.md                     ← This file (project index)
├── FINDINGS-9066.md              ← Protocol reverse-engineering results (153 lines)
├── OSCAR_RE.md                   ← Oscar Editor PE32 binary RE (272 lines)
├── MOUSE-FIRMWARE-RECOVERY.md    ← Chip flashing recovery guide (582 lines)
├── mouse-9066-backup.txt         ← USB descriptors + HID reports (live dump before brick)
├── mouse-backup.txt              ← HID feature report status (all report IDs)
├── oscar-linux/                  ← Linux oscar CLI tool (Zig)
│   ├── src/
│   │   ├── main.zig              ← CLI: list, info, probe, profile, eeprom
│   │   ├── protocol.zig          ← Device defs, EEPROM maps, magic values
│   │   ├── hid.zig               ← libusb HID backend
│   │   ├── hidraw.zig            ← hidraw fallback backend
│   │   └── ini.zig               ← Oscar INI parser
│   ├── build.zig
│   └── zig-out/bin/oscar         ← Pre-built binary
└── OscarX7Editor5Mode/           ← Oscar Editor 5-Mode (Windows PE32)
    ├── OscarEditor.exe           ← Main editor
    └── dll/DLL_MouseDeviceManager.dll ← USB/HID communication DLL
```

Also in `~/Documents/Projects/` (top level):
- `force-port-reset.sh` — Force power-cycle USB port
- `recover-mouse.sh` — Unbind/remove/bind USB device
- `99-x7-mouse.rules` — udev rule for hidraw access
- `session-ses_0b98.md` — Full debugging session transcript
- `/tmp/x7_protocol_test.c` — C protocol test (compile: `gcc -lusb-1.0 -o x7_test x7_protocol_test.c`)

## Key Protocol Findings

The mouse parses standard USB **GET_DESCRIPTOR** (bmRequestType=0x80, bRequest=0x06) with vendor-custom wValue/wIndex to implement a complete EEPROM read/write protocol:

```
Read EEPROM (2B):   ctrl_transfer(0x80, 0x06, 0xb501, magic<<8|addr, buf, 2)
Write byte:         ctrl_transfer(0x80, 0x06, 0xb504, value<<8|offset, buf, 1)
Flush page:         ctrl_transfer(0x80, 0x06, 0xb500, 0x1a00|page, buf, 1)
Block read (8B):    ctrl_transfer(0x80, 0x06, 0xb600, block_num, buf, 8)
Session start/end:  wValue=0xbf01, wIndex=0x0001/0x0002
```

**Critical discovery**: PID 9066 uses magic bytes 0x00-0x07 (NOT 0x1a like standard X7 PID 9090). The block read returns known config data.

## Recovery Path

The mouse does NOT enumerate on USB. Two recovery options:

1. **Chip flashing** ($5-10 for CH341A programmer): Open mouse, locate SPI flash chip, use flashrom to read/write firmware. Guide in `MOUSE-FIRMWARE-RECOVERY.md`.

2. **Replace mouse** (~$15-20): A4Tech X7 F3 V-Track is discontinued but still available on Amazon/eBay.

The oscar Linux CLI tool (Zig) is fully functional for software-level recovery IF the mouse ever enumerates again.

## File Count

| Metric | Count |
|--------|-------|
| Pages of documentation | **3** (FINDINGS + OSCAR_RE + FIRMWARE_RECOVERY) |
| Lines of analysis | **1,007** across all docs |
| Zig CLI tool | **5 source files**, fully functional |
| USB dumps saved | **2** (descriptors + HID reports) |
| Recovery scripts | **3** (port reset, rebind, udev) |
| C protocol test | **1** (`/tmp/x7_protocol_test.c`) |
