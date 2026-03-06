// // Idle detection using GNOME/Mutter D-Bus

#ifndef IDLE_DETECTOR_GNOME_H
#define IDLE_DETECTOR_GNOME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Returns idle time in milliseconds
static long get_idle_time(void) {
    FILE *fp = popen(
        "gdbus call --session "
        "--dest org.gnome.Mutter.IdleMonitor "
        "--object-path /org/gnome/Mutter/IdleMonitor/Core "
        "--method org.gnome.Mutter.IdleMonitor.GetIdletime 2>/dev/null",
        "r"
    );

    if (!fp) return 0;

    char buf[128];
    long idle = 0;

    if (fgets(buf, sizeof(buf), fp)) {
        // Parse "(uint64 12345,)" format
        char *p = strstr(buf, " ");
        if (p) {
            p++; // skip space
            idle = strtol(p, NULL, 10);
        }
    }

    pclose(fp);
    return idle;
}

#endif // IDLE_DETECTOR_GNOME_H
