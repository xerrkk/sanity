#include <libguile.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define FIFO_PATH "/run/gni.fifo"

/* --- Power Management & Cleanup --- */

void kill_the_world() {
    sync();
    printf("\n[GNI] Sending SIGTERM to all processes...\n");
    kill(-1, SIGTERM);
    sleep(2);
    printf("[GNI] Sending SIGKILL to all processes...\n");
    kill(-1, SIGKILL);
    sync();
    // Remount root read-only to prevent filesystem corruption on Ivy Bridge SSD/HDD
    mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
}

void poweroff(int sig) {
    printf("\n[GNI] Powering off...\n");
    kill_the_world();
    reboot(RB_POWER_OFF);
}

void restart(int sig)  {
    printf("\n[GNI] Rebooting system...\n");
    kill_the_world();
    reboot(RB_AUTOBOOT);
}

/* --- Core Initialization --- */

void setup_api_filesystems() {
    printf("[GNI] Mounting API filesystems...\n");
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    
    // Create and mount /run as tmpfs for the FIFO and other volatile data
    mkdir("/run", 0755);
    mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=32M");
    
    // Setup devpts for terminal support
    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "gid=5,mode=620");
}

/* --- Guile VM Handoff & Supervisor Loop --- */

static void inner_main(void *closure, int argc, char **argv) {
    printf("\n[GNI] GNI's Not Init: Guile VM Online.\n");
    
    /* 1. Execute the Master Scheme Director */
    if (access("/etc/gni.scm", R_OK) == 0) {
        scm_c_primitive_load("/etc/gni.scm");
    } else {
        printf("[GNI] ERROR: /etc/gni.scm not found! Dropping to emergency shell...\n");
        system("/bin/sh");
    }

    int fifo_fd = *(int *)closure;
    pid_t getty_pid = -1;

    /* 2. The Supervisor Loop */
    while (1) {
        /* Check IPC FIFO for commands (non-blocking) */
        char buf[64];
        ssize_t cmd_len = read(fifo_fd, buf, sizeof(buf) - 1);
        if (cmd_len > 0) {
            buf[cmd_len] = '\0';
            if (strncmp(buf, "reboot", 6) == 0) restart(0);
            if (strncmp(buf, "halt", 4) == 0) poweroff(0);
            if (strncmp(buf, "off", 3) == 0) poweroff(0);
        }

        /* Keep a login shell alive on TTY1 */
        if (getty_pid <= 0) {
            getty_pid = fork();
            if (getty_pid == 0) {
                setsid();
                int fd = open("/dev/tty1", O_RDWR);
                if (fd >= 0) {
                    ioctl(fd, TIOCSCTTY, 1);
                    dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
                    if (fd > 2) close(fd);
                }
                execl("/sbin/agetty", "agetty", "--noclear", "tty1", "115200", "linux", NULL);
                _exit(1);
            }
        }

        /* The Sub-Reaper: Clean up all orphaned/zombie processes */
        int status;
        pid_t reaped;
        while ((reaped = waitpid(-1, &status, WNOHANG)) > 0) {
            if (reaped == getty_pid) {
                printf("[GNI] TTY1 exited. Respawning...\n");
                getty_pid = -1;
            }
        }

        // 100ms sleep to keep Ivy Bridge CPU usage at ~0%
        usleep(100000); 
    }
}

/* --- Entry Point --- */

int main(int argc, char **argv) {
    // Only allow running as PID 1
    if (getpid() != 1) {
        fprintf(stderr, "GNI must be run as PID 1.\n");
        return 1;
    }

    /* Bootstrap Hardware & FS */
    setup_api_filesystems();
    mount(NULL, "/", NULL, MS_REMOUNT, NULL); // Ensure root is RW

    /* Setup Signals */
    signal(SIGUSR1, poweroff);
    signal(SIGINT,  restart); // Handle Ctrl-Alt-Del
    reboot(RB_DISABLE_CAD);   // Let us handle the signal ourselves

    /* Setup IPC FIFO */
    unlink(FIFO_PATH);
    if (mkfifo(FIFO_PATH, 0600) == -1) {
        perror("[GNI] Failed to create FIFO");
    }
    int fifo_fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);

    printf("\n[GNI] GNI's Not Init v1.0 starting up...\n");

    /* Boot the Guile VM and jump to inner_main */
    scm_boot_guile(argc, argv, inner_main, &fifo_fd);

    return 0; // Never reached
}
