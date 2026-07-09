#!/usr/bin/env zsh
set -euo pipefail

BRIDGE_DIR="$HOME/Documents/lovense-equinox-bridge"
PROJECT_SRC="/tmp/opencode/lovense-equinox-bridge"
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

msg()  { printf "${BLUE}::${NC} %s\n" "$*"; }
ok()   { printf "${GREEN}OK${NC}  %s\n" "$*"; }
warn() { printf "${YELLOW}!!${NC} %s\n" "$*"; }
fail() { printf "${RED}ERR${NC} %s\n" "$*"; exit 1; }

msg "Lovense Equinox Bridge — Installer"
msg "Target: $BRIDGE_DIR"

# ---- 1. Check Python ----
command -v python3 >/dev/null 2>&1 || fail "python3 not found. Install Python 3.10+ first."

# ---- 2. Check & install pip dependencies ----
msg "Installing Python packages..."
pip3 install aiohttp bleak buttplug 2>&1 | tail -5
ok "Dependencies installed"

# ---- 3. Copy project files ----
if [[ ! -d "$PROJECT_SRC" ]]; then
    fail "Project source not found at $PROJECT_SRC. Run the bridge builder first."
fi

mkdir -p "$BRIDGE_DIR"
cp "$PROJECT_SRC"/*.py "$BRIDGE_DIR/"
cp "$PROJECT_SRC"/requirements.txt "$BRIDGE_DIR/"
cp "$PROJECT_SRC"/lovense-bridge.user.js "$BRIDGE_DIR/"
cp "$PROJECT_SRC"/README.md "$BRIDGE_DIR/"

ok "Project copied to $BRIDGE_DIR"

# ---- 4. BLE permissions (Linux) ----
if [[ "$(uname)" == "Linux" ]]; then
    msg "Setting up BLE permissions..."
    PYTHON_BIN="$(command -v python3)"

    # Method A: setcap (effective immediately, but may break Python imports on some systems)
    if command -v setcap >/dev/null 2>&1; then
        if sudo -n true 2>/dev/null; then
            sudo setcap cap_net_raw,cap_net_admin+eip "$PYTHON_BIN" 2>/dev/null || true
            ok "setcap applied to $PYTHON_BIN"
        else
            warn "sudo not available. To run without root, apply setcap manually:"
            warn "  sudo setcap cap_net_raw,cap_net_admin+eip $(which python3)"
        fi
    fi

    # Method B: udev rule for Bluetooth (persistent, applies to all users)
    UDEV_FILE="/etc/udev/rules.d/99-bluetooth-capabilities.rules"
    if [[ ! -f "$UDEV_FILE" ]] && sudo -n true 2>/dev/null; then
        echo 'KERNEL=="hci[0-9]*", SUBSYSTEM=="bluetooth", ACTION=="add", RUN+="/bin/setcap cap_net_raw,cap_net_admin+eip %k"' \
            | sudo tee "$UDEV_FILE" >/dev/null 2>&1 || true
        ok "Created udev rule at $UDEV_FILE"
    fi

    # Suggest user group membership
    if groups | grep -q '\bbluetooth\b'; then
        ok "User is in the 'bluetooth' group"
    else
        warn "Consider adding yourself to the 'bluetooth' group to avoid root:"
        warn "  sudo usermod -aG bluetooth $USER"
        warn "  (log out and back in for it to take effect)"
    fi
fi

# ---- 5. Create launcher alias ----
ALIAS_FILE="$HOME/.zshrc"
ALIAS_LINE='alias lovense-bridge="cd $HOME/Documents/lovense-equinox-bridge && python3 bridge.py"'

if grep -q "lovense-bridge" "$ALIAS_FILE" 2>/dev/null; then
    ok "Shell alias already exists in $ALIAS_FILE"
else
    printf "\n# Lovense Equinox Bridge\n%s\n" "$ALIAS_LINE" >> "$ALIAS_FILE"
    ok "Added alias 'lovense-bridge' to $ALIAS_FILE"
fi

# ---- 6. Summary ----
printf "\n${GREEN}═══════════════════════════════════════${NC}\n"
printf "${GREEN}  Install complete!${NC}\n"
printf "${GREEN}═══════════════════════════════════════${NC}\n"
echo ""
msg "Start the bridge:"
echo "  lovense-bridge --mode ble"
echo ""
msg "Or manually:"
echo "  cd $BRIDGE_DIR && python3 bridge.py --mode ble"
echo ""
msg "Tampermonkey userscript:"
echo "  Open Tampermonkey → Create a new script → paste contents of:"
echo "  $BRIDGE_DIR/lovense-bridge.user.js"
echo ""
msg "Full docs:"
echo "  $BRIDGE_DIR/README.md"
echo ""
msg "Test the API (with bridge running):"
echo "  curl -X POST http://127.0.0.1:20010/command \\"
echo '    -H "Content-Type: application/json" \\'
echo '    -d "{\\"command\\":\\"GetToys\\"}"'
