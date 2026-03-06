/*
 * JIGGLEMIL - Native C Daemon (Wayland Jiggler)
 *
 * Human-like mouse movement using WindMouse algorithm.
 * Keeps your status green by injecting kernel-level input events.
 *
 * Author: Emil Szymecki + Claude
 * License: MIT
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "config.h"

#ifdef USE_GNOME_IDLE
#include "idle_detector_gnome.h"
#else
#include "idle_detector_libinput.h"
#endif



// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    int dx;
    int dy;
    int delay_us;
} PathPoint;

typedef struct {
    PathPoint points[MAX_PATH_POINTS];
    int count;
} MousePath;

// ============================================================================
// GLOBAL STATE
// ============================================================================

volatile sig_atomic_t g_running = 1;
int g_smooth_mode = 0;
int g_watch_mode = 0;

// ============================================================================
// SIGNAL HANDLING
// ============================================================================

void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

void setup_signals(void) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

// ============================================================================
// LOGGING & STATE
// ============================================================================

void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%H:%M:%S", t);
}

void log_msg(const char *msg) {
    char ts[16];
    get_timestamp(ts, sizeof(ts));

    FILE *fp = fopen(LOG_FILE, "a");
    if (fp) {
        fprintf(fp, "%s | %s\n", ts, msg);
        fclose(fp);
    }
}

// Watch mode display
void display_watch(const char *status, const char *emoji, long idle_ms, long action_limit) {
    if (!g_watch_mode) return;

    long idle_sec = idle_ms / 1000;
    long warning_sec = WARNING_LIMIT_MS / 1000;  // 30s

    // Calculate timers
    long green_left = warning_sec - idle_sec;  // time until red
    if (green_left < 0) green_left = 0;

    long red_left = (action_limit - idle_ms) / 1000;  // time until action
    if (red_left < 0) red_left = 0;

    char ts[16];
    get_timestamp(ts, sizeof(ts));

    // Clear screen and move cursor to top
    printf("\033[2J\033[H");

    printf("═══════════════════════════════════════════\n");
    printf("       JIGGLEMIL - WATCH MODE\n");
    printf("═══════════════════════════════════════════\n");
    printf("\n");
    printf("  %s  %s\n", emoji, status);
    printf("\n");

    if (idle_sec < warning_sec) {
        // Green phase - show countdown to red
        printf("      Green:  %2ld s  ↓\n", green_left);
        printf("      Red:    --\n");
    } else {
        // Red phase - show countdown to action
        printf("      Green:   0 s\n");
        printf("      Red:   %2ld s  ↓\n", red_left);
    }

    printf("\n");
    printf("═══════════════════════════════════════════\n");
    printf("  [%s]  Mode: %s\n", ts, g_smooth_mode ? "SMOOTH" : "BATCH");
    printf("═══════════════════════════════════════════\n");
    printf("\n");
    printf("  Press Ctrl+C to stop\n");

    fflush(stdout);
}

void save_state(const char *emoji) {
    FILE *fp = fopen(STATE_FILE, "w");
    if (fp) {
        fprintf(fp, "%s", emoji);
        fclose(fp);
    }
}

void save_pid(void) {
    FILE *fp = fopen(PID_FILE, "w");
    if (fp) {
        fprintf(fp, "%d", getpid());
        fclose(fp);
    }
}

void remove_pid(void) {
    unlink(PID_FILE);
}


// ============================================================================
// WINDMOUSE PATH GENERATOR (Pure function, no side effects)
// ============================================================================

static double randf(double min, double max) {
    return min + ((double)rand() / RAND_MAX) * (max - min);
}

MousePath generate_wind_path(double target_x, double target_y) {
    MousePath path = {0};

    // Randomize parameters for this movement (each path is unique)
    double mouse_speed   = randf(MOUSE_SPEED_MIN, MOUSE_SPEED_MAX);
    double gravity       = randf(GRAVITY_MIN, GRAVITY_MAX);
    double wind          = randf(WIND_MIN, WIND_MAX);
    double target_radius = randf(TARGET_RADIUS_MIN, TARGET_RADIUS_MAX);
    double max_step      = randf(MAX_STEP_MIN, MAX_STEP_MAX);

    double x = 0, y = 0;
    double vx = 0, vy = 0;
    double wx = 0, wy = 0;

    while (hypot(target_x - x, target_y - y) > target_radius
           && path.count < MAX_PATH_POINTS) {

        double dist = hypot(target_x - x, target_y - y);

        // Wind component (random drift)
        wx = wx / sqrt(3.0) + randf(-wind, wind) / sqrt(5.0);
        wy = wy / sqrt(3.0) + randf(-wind, wind) / sqrt(5.0);

        // Velocity update (gravity pulls toward target)
        if (dist > 0) {
            vx += (wx + (target_x - x) * gravity / dist) / mouse_speed;
            vy += (wy + (target_y - y) * gravity / dist) / mouse_speed;
        }

        // Clamp velocity
        double vel = hypot(vx, vy);
        if (vel > max_step) {
            vx = (vx / vel) * max_step;
            vy = (vy / vel) * max_step;
        }

        // Calculate pixel delta
        int dx = (int)round(x + vx) - (int)round(x);
        int dy = (int)round(y + vy) - (int)round(y);

        x += vx;
        y += vy;

        // Only record actual movements
        if (dx != 0 || dy != 0) {
            path.points[path.count].dx = dx;
            path.points[path.count].dy = dy;
            path.points[path.count].delay_us = (int)randf(MIN_DELAY_US, MAX_DELAY_US);
            path.count++;
        }
    }

    return path;
}

// ============================================================================
// PATH EXECUTOR (I/O layer)
// ============================================================================

// Execute single ydotool command via fork/exec (no shell)
static int exec_ydotool(char *const argv[]) {
    pid_t pid = fork();

    if (pid < 0) {
        return -1;
    } else if (pid == 0) {
        // Child process - redirect stdout/stderr to /dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp("ydotool", argv);
        _exit(127);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
}

// Batch mode: fast execution with minimal delays
void execute_path_batch(const MousePath *path) {
    if (path->count == 0) return;

    char cmd[128];
    for (int i = 0; i < path->count && g_running; i++) {
        snprintf(cmd, sizeof(cmd), "ydotool mousemove -- %d %d",
                 path->points[i].dx, path->points[i].dy);
        system(cmd);
        usleep(5000);  // 5ms between moves
    }
}

// Smooth mode: individual movements with delays (more human-like)
void execute_path_smooth(const MousePath *path) {
    for (int i = 0; i < path->count && g_running; i++) {
        char dx_str[16], dy_str[16];
        snprintf(dx_str, sizeof(dx_str), "%d", path->points[i].dx);
        snprintf(dy_str, sizeof(dy_str), "%d", path->points[i].dy);

        char *argv[] = {"ydotool", "mousemove", "--", dx_str, dy_str, NULL};
        exec_ydotool(argv);

        usleep(path->points[i].delay_us);
    }
}

void execute_path(const MousePath *path) {
    if (g_smooth_mode) {
        execute_path_smooth(path);
    } else {
        execute_path_batch(path);
    }
}

// ============================================================================
// MAIN ACTION
// ============================================================================

void perform_wind_move(void) {
    // Random target - larger range = longer path with more waves
    double tx = randf(-400, 400);
    double ty = randf(-400, 400);

    char msg[128];
    snprintf(msg, sizeof(msg), "    -> Target: (%.0f, %.0f)", tx, ty);
    log_msg(msg);

    MousePath path = generate_wind_path(tx, ty);

    snprintf(msg, sizeof(msg), "    -> Path: %d points", path.count);
    log_msg(msg);

    execute_path(&path);
}

// ============================================================================
// NOTIFICATION (optional, non-blocking)
// ============================================================================

void notify(const char *title, const char *body) {
    pid_t pid = fork();
    if (pid == 0) {
        // Redirect to /dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("notify-send", "notify-send", title, body, NULL);
        _exit(0);
    }
    // Don't wait - fire and forget
}

// ============================================================================
// USAGE
// ============================================================================

void print_usage(const char *prog) {
    printf("Jigglemil - Keep your status green\n\n");
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --watch      Live dashboard mode (see status in real-time)\n");
    printf("  --smooth     Use smooth mode (individual moves with delays)\n");
    printf("  --help       Show this help\n");
    printf("\n");
    printf("Control:\n");
    printf("  Kill with: pkill jigglemil  or  kill $(cat /tmp/jigglemil.pid)\n");
    printf("\n");
    printf("Status:\n");
    printf("  cat /tmp/jigglemil.state    # green/red/white/black\n");
    printf("  tail -f /tmp/jigglemil.log  # live logs\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--smooth") == 0) {
            g_smooth_mode = 1;
        } else if (strcmp(argv[i], "--watch") == 0) {
            g_watch_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    // Initialize idle detector
    init_idle_detector();   // <-- NEW LINE

    srand(time(NULL) ^ getpid());
    setup_signals();
    save_pid();

    // Set ydotool socket path
    setenv("YDOTOOL_SOCKET", "/tmp/.ydotool_socket", 1);

    // Clear/init log
    FILE *fp = fopen(LOG_FILE, "w");
    if (fp) fclose(fp);

    // Randomize first action threshold
    long action_limit = MIN_ACTION_MS + (rand() % (MAX_ACTION_MS - MIN_ACTION_MS));

    // Startup
    log_msg("═══════════════════════════════════════");
    log_msg("JIGGLEMIL STARTED");
    log_msg(g_smooth_mode ? "    Mode: SMOOTH" : "    Mode: BATCH");

    char msg[128];
    snprintf(msg, sizeof(msg), "    First trigger: %lds", action_limit / 1000);
    log_msg(msg);
    log_msg("═══════════════════════════════════════");

    save_state("🟢");
    notify("Jigglemil", "Running");

    // Show initial watch display
    if (g_watch_mode) {
        display_watch("STARTING", "🟢", 0, action_limit);
    }

    // ========================================================================
    // MAIN LOOP
    // ========================================================================

    while (g_running) {
        long idle_ms = get_idle_time();

        if (idle_ms > action_limit) {
            // === ACTION (WHITE) ===
            save_state("🟡");
            display_watch("ACTION!", "🟡", idle_ms, action_limit);

            snprintf(msg, sizeof(msg), "ACTION! Idle: %lds / Limit: %lds",
                     idle_ms / 1000, action_limit / 1000);
            log_msg(msg);

            perform_wind_move();

            // Randomize next threshold
            action_limit = MIN_ACTION_MS + (rand() % (MAX_ACTION_MS - MIN_ACTION_MS));

            snprintf(msg, sizeof(msg), "Done. Next trigger: %lds", action_limit / 1000);
            log_msg(msg);

            save_state("🟢");

            // Wait for system to register activity
            sleep(3);

        } else if (idle_ms > WARNING_LIMIT_MS) {
            // === WARNING (RED) ===
            save_state("🔴");
            display_watch("WARNING", "🔴", idle_ms, action_limit);

        } else {
            // === SAFE (GREEN) ===
            save_state("🟢");
            display_watch("SAFE", "🟢", idle_ms, action_limit);
        }

        sleep(CHECK_INTERVAL_SEC);
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    log_msg("═══════════════════════════════════════");
    log_msg("JIGGLEMIL STOPPED (signal received)");
    log_msg("═══════════════════════════════════════");

    save_state("⚫");
    remove_pid();
    notify("Jigglemil", "Stopped");

    return 0;
}
