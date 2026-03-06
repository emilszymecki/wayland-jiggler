# Jigglemil

> **Keep your status Green.** Fight the micromanagement. Stay active.

A native C daemon that keeps your session alive using human-like mouse movements. Works on **Wayland** where other tools fail.

![Demo](demo.gif)

## Why This Works (And Others Don't)

| Tool | Method | Detectable? |
|------|--------|-------------|
| Caffeine | Inhibits screensaver via D-Bus | **Trivial** - just a flag |
| xdotool | X11 synthetic events | **Easy** - tagged as synthetic |
| Mouse jigglers (USB) | Hardware HID events | Detectable by device ID |
| **Jigglemil** | uinput kernel events | **Indistinguishable from real hardware** |

### The Secret Sauce

1. **Kernel-level injection** - Uses `ydotool` which writes directly to `/dev/uinput`. From the kernel's perspective, this IS a real input device.

2. **WindMouse algorithm** - Borrowed from game bot research. Creates curved, organic trajectories with simulated "wind" and "gravity" instead of robotic straight lines.

3. **Randomized everything** - Parameters randomized per-movement: speed, gravity, wind, timing. No two movements are alike. Zero temporal signature.

## Quick Start

```bash
git clone https://github.com/emilszymecki/wayland-jiggler.git
cd wayland-jiggler
sudo ./install.sh

# Run it
jiggler --start
```

## Usage

```bash
jiggler --start    # Start daemon (smooth mode)
jiggler --stop     # Stop daemon
jiggler --toggle   # Toggle on/off (for shortcuts/icons)
jiggler --status   # Show current state (🟢/🔴/🟡/⚫)
jiggler --watch    # Live dashboard
```

### As a service (auto-start on login)
```bash
systemctl --user enable --now jigglemil
```

## Status Indicators

| Emoji | State | Meaning |
|-------|-------|---------|
| 🟢 | Safe | User active or monitoring |
| 🔴 | Warning | Idle > 30s, action coming |
| 🟡 | Action | Performing mouse movement |
| ⚫ | Stopped | Daemon not running |

## GNOME Integration (Executor Extension)

1. Install [Executor](https://extensions.gnome.org/extension/2932/executor/)
2. Add command:
   - **Command:** `jiggler --status`
   - **Interval:** `1`
   - **Left Click:** `jiggler --toggle`

Result: Live status dot in your top bar. Click to toggle.

## How It Works

```
  0-30s idle     →  🟢 Green (safe)
  30-60s idle    →  🔴 Red (warning)
  60-120s idle   →  🟡 Action (random trigger)
       ↓
  Mouse moves with WindMouse algorithm
       ↓
  Idle resets to 0, cycle repeats
```

### WindMouse Algorithm

Instead of straight lines, generates organic S-curves:
- **Gravity** - pulls toward target
- **Wind** - random drift for natural wobble
- **Speed** - controls iteration count

Each movement randomizes all parameters → 100-400 unique path points.

## Monitoring

```bash
# Live dashboard
jiggler --watch

# Live logs
tail -f /tmp/jigglemil.log

# Current status
jiggler --status
```

## Configuration

Edit `src/config.h` and recompile:

```c
// Timing (ms)
#define WARNING_LIMIT_MS    30000   // Red starts at 30s
#define MIN_ACTION_MS       60000   // Action earliest at 60s
#define MAX_ACTION_MS       120000  // Action latest at 120s

// WindMouse (randomized ranges per movement)
#define MOUSE_SPEED_MIN     20.0
#define MOUSE_SPEED_MAX     30.0
#define GRAVITY_MIN         3.0
#define GRAVITY_MAX         5.0
#define WIND_MIN            6.0
#define WIND_MAX            10.0
```

Recompile: `sudo ./install.sh`

## Troubleshooting

### Mouse not moving after reboot
```bash
# Check ydotoold service
systemctl status ydotoold

# Restart it
sudo systemctl restart ydotoold
```

### "failed to connect socket"
```bash
# Reinstall (fixes ydotoold config)
sudo ./install.sh
```

### Manual ydotool test
```bash
export YDOTOOL_SOCKET=/tmp/.ydotool_socket
ydotool mousemove -- 50 50
```

## License

MIT License

## Credits

- **WindMouse algorithm** - Adapted from Ben Land's implementation
- **ydotool** - Kernel-level input injection
- **Claude** - Pair programming partner

---

*Made with mass distrust of activity monitoring*
