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

#define FIFO_PATH "/run/sherpa.fifo"

/* --- Power Management --- */

void kill_the_world() {
    sync();
    printf("\n** Terminating all processes... **\n");
    kill(-1, SIGTERM);
    sleep(2);
    kill(-1, SIGKILL);
    sync();
    // Ivy Bridge / SSD Safety: Final flush and remount RO
    mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
}

void poweroff_handler(int sig) {
    kill_the_world();
    reboot(RB_POWER_OFF);
}

void reboot_handler(int sig) {
    kill_the_world();
    reboot(RB_AUTOBOOT);
}

/* --- System Prep --- */

void setup_api_filesystems() {
    printf("** Mounting kernel API filesystems... **\n");
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    
    mkdir("/run", 0755);
    mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=32M");

    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "gid=5,mode=620");
}

/* --- The Guile/Supervisor Core --- */

static void inner_main(void *closure, int argc, char **argv) {
    printf("[SHERPK] Entering Guile VM...\n");

    /* Load the Master Script */
    if (access("/etc/sherpa.scm", R_OK) == 0) {
        scm_c_primitive_load("/etc/sherpa.scm");
    } else {
        printf("** /etc/sherpa.scm missing! /bin/sh? **\n");
        system("/bin/sh");
    }

    int fifo_fd = *(int *)closure;
    pid_t getty_pid = -1;

    /* Supervisor Loop */
    while (1) {
        // 1. Check IPC for commands
        char buf[64];
        ssize_t cmd_len = read(fifo_fd, buf, sizeof(buf) - 1);
        if (cmd_len > 0) {
            buf[cmd_len] = '\0';
            if (strstr(buf, "reboot")) reboot_handler(0);
            if (strstr(buf, "halt") || strstr(buf, "off")) poweroff_handler(0);
        }

        // 2. Keep TTY1 Alive (The Shepherd's Watch)
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

        // 3. Sub-Reaper: Clean up zombies
        int status;
        pid_t reaped;
        while ((reaped = waitpid(-1, &status, WNOHANG)) > 0) {
            if (reaped == getty_pid) {
                printf("** Teletype 1 died. Restarting... **\n");
                getty_pid = -1;
            }
        }

        usleep(100000); // 100ms duty cycle
    }
}

/* --- Entry --- */

int main(int argc, char **argv) {
    if (getpid() != 1) {
        fprintf(stderr, "** Must be ran as PID 1. **\n");
        return 1;
    }

    setup_api_filesystems();
    mount(NULL, "/", NULL, MS_REMOUNT, NULL); // Go RW

    // Signal Management
    signal(SIGUSR1, poweroff_handler);
    signal(SIGINT,  reboot_handler); // Ctrl-Alt-Del
    reboot(RB_DISABLE_CAD);

    // IPC Setup
    unlink(FIFO_PATH);
    mkfifo(FIFO_PATH, 0600);
    int fifo_fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);

    printf("\n** sherpa - a fine man\'s init **\n");

    scm_boot_guile(argc, argv, inner_main, &fifo_fd);

    return 0; 
}
