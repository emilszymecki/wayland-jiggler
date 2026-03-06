#!/bin/bash
set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Jigglemil - Installation               ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo

# ============================================================================
# STEP 0: Determine installation context
# ============================================================================

# Check root for system-wide install
INSTALL_USER=0
if [ "$EUID" -ne 0 ]; then
    echo -e "${YELLOW}⚠ Not running as root. Installing to ~/.local/bin${NC}"
    INSTALL_USER=1
    PREFIX="$HOME/.local"
    mkdir -p "$PREFIX/bin"
else
    PREFIX="/usr/local"
fi

# Detect user for adding to input group
if [ "$SUDO_USER" ]; then
    ACTUAL_USER="$SUDO_USER"
else
    ACTUAL_USER="$USER"
fi

# ============================================================================
# STEP 0.5: Detect package manager
# ============================================================================

if command -v pacman &> /dev/null; then
    PKG_MGR="pacman"
    INSTALL_CMD="sudo pacman -S --needed --noconfirm"
elif command -v apt &> /dev/null; then
    PKG_MGR="apt"
    INSTALL_CMD="sudo apt install -y"
elif command -v dnf &> /dev/null; then
    PKG_MGR="dnf"
    INSTALL_CMD="sudo dnf install -y"
elif command -v zypper &> /dev/null; then
    PKG_MGR="zypper"
    INSTALL_CMD="sudo zypper install -y"
else
    PKG_MGR=""
    INSTALL_CMD=""
fi

if [ -z "$PKG_MGR" ]; then
    echo -e "${RED}✗ No supported package manager detected.${NC}"
    echo "  Please install dependencies manually."
    exit 1
else
    echo -e "${GREEN}✓${NC} Detected package manager: $PKG_MGR"
fi

# ============================================================================
# STEP 1: Dependencies
# ============================================================================

echo -e "${YELLOW}[1/4]${NC} Checking dependencies..."

# Check GCC / build essentials
if ! command -v gcc &> /dev/null; then
    echo -e "  ${RED}✗${NC} gcc not found"
    if [ "$INSTALL_USER" -eq 0 ]; then
        echo "    Installing build tools..."
        $INSTALL_CMD base-devel || $INSTALL_CMD build-essential
    else
        echo -e "    ${RED}Please install gcc/make manually${NC}"
        exit 1
    fi
else
    echo -e "  ${GREEN}✓${NC} gcc found"
fi

# Check ydotool and find ydotoold path
YDOTOOLD_BIN=""
if command -v ydotoold &> /dev/null; then
    YDOTOOLD_BIN="$(command -v ydotoold)"
    echo -e "  ${GREEN}✓${NC} ydotoold found ($YDOTOOLD_BIN)"
elif [ -x "/usr/local/bin/ydotoold" ]; then
    YDOTOOLD_BIN="/usr/local/bin/ydotoold"
    echo -e "  ${GREEN}✓${NC} ydotoold found ($YDOTOOLD_BIN)"
elif [ -x "/usr/bin/ydotoold" ]; then
    YDOTOOLD_BIN="/usr/bin/ydotoold"
    echo -e "  ${GREEN}✓${NC} ydotoold found ($YDOTOOLD_BIN)"
else
    echo -e "  ${RED}✗${NC} ydotoold not found"
    if [ "$INSTALL_USER" -eq 0 ]; then
        echo "    Installing ydotool..."
        $INSTALL_CMD ydotool
        YDOTOOLD_BIN="/usr/bin/ydotoold"
    else
        echo -e "    ${RED}Please install ydotool manually${NC}"
        exit 1
    fi
fi

# Check ydotool client
if ! command -v ydotool &> /dev/null; then
    echo -e "  ${RED}✗${NC} ydotool client not found"
    exit 1
else
    echo -e "  ${GREEN}✓${NC} ydotool client found"
fi

# Check notify-send (optional)
if command -v notify-send &> /dev/null; then
    echo -e "  ${GREEN}✓${NC} notify-send found"
else
    echo -e "  ${YELLOW}○${NC} notify-send not found (optional)"
fi

# ============================================================================
# STEP 2: Compile
# ============================================================================

echo
echo -e "${YELLOW}[2/4]${NC} Compiling..."

cd "$(dirname "$0")"

if [ ! -f "src/jigglemil.c" ]; then
    echo -e "  ${RED}✗${NC} src/jigglemil.c not found!"
    exit 1
fi

USE_GNOME_IDLE_FLAG=""

# Check if libinput is available
if pkg-config --exists libinput; then
    echo -e "  ${GREEN}✓${NC} libinput detected - using libinput idle method"
elif [ -e /dev/input/event0 ]; then
    echo -e "  ${GREEN}✓${NC} /dev/input devices found - using libinput idle method"
else
    # fallback to GNOME idle detection
    echo -e "  ${YELLOW}⚠ libinput not found - using GNOME idle method${NC}"
    USE_GNOME_IDLE_FLAG="-DUSE_GNOME_IDLE"
fi

gcc -Wall -Wextra -O2 -std=c11 src/jigglemil.c -lm -linput -ludev $USE_GNOME_IDLE_FLAG -o jigglemil
echo -e "  ${GREEN}✓${NC} Compilation successful"

# ============================================================================
# STEP 3: Install binary
# ============================================================================

echo
echo -e "${YELLOW}[3/4]${NC} Installing binary..."

if [ "$INSTALL_USER" -eq 0 ]; then
    install -Dm755 jigglemil "$PREFIX/bin/jigglemil"
else
    install -m755 jigglemil "$PREFIX/bin/jigglemil"
fi
echo -e "  ${GREEN}✓${NC} Installed to $PREFIX/bin/jigglemil"

# Cleanup build artifact
rm -f jigglemil

# Install wrapper script
if [ -f "jiggler" ]; then
    if [ "$INSTALL_USER" -eq 0 ]; then
        install -Dm755 jiggler "$PREFIX/bin/jiggler"
    else
        install -m755 jiggler "$PREFIX/bin/jiggler"
    fi
    echo -e "  ${GREEN}✓${NC} Wrapper installed to $PREFIX/bin/jiggler"
fi

# ============================================================================
# STEP 4: Setup ydotoold service (if root)
# ============================================================================

echo
echo -e "${YELLOW}[4/4]${NC} Configuring ydotoold service..."

if [ "$INSTALL_USER" -eq 0 ]; then
    # Create systemd service for ydotoold (using detected path)
    cat > /etc/systemd/system/ydotoold.service << EOF
[Unit]
Description=ydotool daemon
Documentation=man:ydotoold(8)
After=network.target

[Service]
Type=simple
Restart=always
RestartSec=3
ExecStart=$YDOTOOLD_BIN --socket-path=/tmp/.ydotool_socket
ExecStartPost=/usr/bin/sleep 1
ExecStartPost=/usr/bin/chmod 0666 /tmp/.ydotool_socket

[Install]
WantedBy=multi-user.target
EOF

    # Reload and enable
    systemctl daemon-reload
    systemctl enable --now ydotoold 2>/dev/null || true

    echo -e "  ${GREEN}✓${NC} ydotoold service configured"
else
    echo -e "  ${YELLOW}○${NC} Skipped (need root for systemd)"
    echo -e "    ${YELLOW}Make sure ydotoold is running:${NC}"
    echo -e "    sudo systemctl enable --now ydotoold"
fi

# ============================================================================
# STEP 5: Optional - User systemd service for jigglemil
# ============================================================================

SYSTEMD_USER_DIR="$HOME/.config/systemd/user"
mkdir -p "$SYSTEMD_USER_DIR"

cat > "$SYSTEMD_USER_DIR/jigglemil.service" << EOF
[Unit]
Description=Jigglemil - Keep your status green
Documentation=https://github.com/emilszymecki/wayland-jiggler
After=graphical-session.target

[Service]
Type=simple
ExecStart=$PREFIX/bin/jigglemil --smooth
Restart=on-failure
RestartSec=5
Environment=YDOTOOL_SOCKET=/tmp/.ydotool_socket

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload 2>/dev/null || true
echo -e "  ${GREEN}✓${NC} User service installed: ~/.config/systemd/user/jigglemil.service"

# ============================================================================
# STEP 6: Ensure user is in 'input' group for libinput method
# ============================================================================

if [ -f "src/idle_detector_libinput.h" ]; then
    echo -e "${YELLOW}[!]${NC} Libinput method detected, checking 'input' group..."
    if id -nG "$ACTUAL_USER" | grep -qw "input"; then
        echo -e "  ${GREEN}✓${NC} User '$ACTUAL_USER' is already in 'input' group"
    else
        echo -e "  ${YELLOW}⚠ User '$ACTUAL_USER' is not in 'input' group. Adding..."
        sudo usermod -aG input "$ACTUAL_USER"
        echo "    ✅ Added. Log out and log back in for changes to take effect."
    fi
fi

# ============================================================================
# DONE
# ============================================================================

echo
echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   ✅ Installation Complete!              ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo
echo "Usage:"
echo -e "  ${YELLOW}jiggler --start${NC}        # Start daemon"
echo -e "  ${YELLOW}jiggler --stop${NC}         # Stop daemon"
echo -e "  ${YELLOW}jiggler --toggle${NC}       # Toggle on/off (for shortcuts)"
echo -e "  ${YELLOW}jiggler --status${NC}       # Show state (for Executor)"
echo -e "  ${YELLOW}jiggler --watch${NC}        # Live dashboard"
echo
if [[ "$XDG_CURRENT_DESKTOP" =~ GNOME ]]; then
    echo
    echo "GNOME Executor extension:"
    echo -e "  Command:     ${YELLOW}jiggler --status${NC}"
    echo -e "  Left Click:  ${YELLOW}jiggler --toggle${NC}"
    echo
fi
echo "Monitor:"
echo -e "  ${YELLOW}tail -f /tmp/jigglemil.log${NC}           # Live logs"
echo
