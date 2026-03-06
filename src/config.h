// config.h
// Jigglemil configuration file (Wayland / KDE / Arch Linux)

// ============================================================================
// FILE PATHS
// ============================================================================
#define STATE_FILE      "/tmp/jigglemil.state"
#define LOG_FILE        "/tmp/jigglemil.log"
#define PID_FILE        "/tmp/jigglemil.pid"

// ============================================================================
// TIMERS (in milliseconds)
// ============================================================================
#define WARNING_LIMIT_MS    30000       // 30s  - red warning starts
#define MIN_ACTION_MS       87000       // 87s  - minimum idle before action
#define MAX_ACTION_MS       180000      // 180s - maximum idle before action
#define CHECK_INTERVAL_SEC  1           // how often to check idle time

// ============================================================================
// WINDMOUSE PARAMETERS (randomized for human-like variance)
// ============================================================================
#define MOUSE_SPEED_MIN     26.0
#define MOUSE_SPEED_MAX     43.0
#define GRAVITY_MIN         3.0
#define GRAVITY_MAX         5.0
#define WIND_MIN            23.0
#define WIND_MAX            78.0
#define TARGET_RADIUS_MIN   3.0
#define TARGET_RADIUS_MAX   7.0
#define MAX_STEP_MIN        2.0
#define MAX_STEP_MAX        4.0
#define MAX_PATH_POINTS     1783
#define MIN_DELAY_US        5000
#define MAX_DELAY_US        15000
