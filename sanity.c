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

#define FIFO_PATH "/run/gni.fifo"

/* --- Power & Handoff Logic --- */
void kill_the_world() {
    sync();
    kill(-1, SIGTERM);
    sleep(2);
    kill(-1, SIGKILL);
    sync();
    mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
}

void poweroff(int sig) { kill_the_world(); reboot(RB_POWER_OFF); }
void restart(int sig)  { kill_the_world(); reboot(RB_AUTOBOOT); }

static void inner_main(void *closure, int argc, char **argv) {
    printf("\n[GNI] GNI's Not Init: Invoking the Scheme Soul...\n");
    
    // Load the Master Director
    scm_c_primitive_load("/etc/gni.scm");

    int fifo_fd = *(int *)closure;
    pid_t getty_pid = -1;

    while (1) {
        char buf[64];
        if (read(fifo_fd, buf, sizeof(buf) - 1) > 0) {
            if (strncmp(buf, "reboot", 6) == 0) restart(0);
            if (strncmp(buf, "halt", 4) == 0) poweroff(0);
        }

        /* GNI Supervision: Keep Getty alive on TTY1 */
        if (getty_pid <= 0) {
            getty_pid = fork();
            if (getty_pid == 0) {
                setsid();
                int fd = open("/dev/tty1", O_RDWR);
                ioctl(fd, TIOCSCTTY, 1);
                dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
                execl("/sbin/agetty", "agetty", "--noclear", "tty1", "115200", "linux", NULL);
                _exit(1);
            }
        }
        // Zombie Reaper
        while (waitpid(-1, NULL, WNOHANG) > 0);
        usleep(100000); 
    }
}

int main(int argc, char **argv) {
    if (getpid() != 1) return 1;

    // Phase 1: Minimal hardware bootstrap
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/run", 0755);
    mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=32M");
    mount(NULL, "/", NULL, MS_REMOUNT, NULL); // Go Read/Write

    signal(SIGUSR1, poweroff);
    signal(SIGINT,  restart);

    mkfifo(FIFO_PATH, 0600);
    int fifo_fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);

    // Phase 2: Boot Guile
    scm_boot_guile(argc, argv, inner_main, &fifo_fd);
    
    return 0;
}
