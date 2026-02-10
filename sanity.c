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

/* --- Prototypes --- */
void poweroff(int sig);
void restart(int sig);
void run(char *cmd, char *args[]);
void setup_api_filesystems();
void setup_terminal_subsystem();
void setup_hardware();
void setup_identity();
void setup_network();

/* --- Power Management --- */

void poweroff(int sig) { 
    printf("\n** Shutting down... **\n");
    sync();
    reboot(RB_POWER_OFF); 
}

void restart(int sig)  { 
    printf("\n** Rebooting... **\n");
    sync();
    reboot(RB_AUTOBOOT); 
}

/* --- Helpers --- */

void run(char *cmd, char *args[]) {
    if (access(cmd, X_OK) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        execv(cmd, args);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
}

/* --- Subsystem Modules --- */

void setup_api_filesystems() {
    printf("** Mounting API filesystems... **\n");
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    
    mkdir("/run", 0755);
    mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=32M");
    mkdir("/dev/shm", 0755);
    mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
}

void setup_terminal_subsystem() {
    printf("** Setting up devpts... **\n");
    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "gid=5,mode=620");
}

void setup_hardware() {
    printf("** Starting udevd and triggering scan... **\n");
    pid_t udev_pid = fork();
    if (udev_pid == 0) {
        char *udev_args[] = {"/sbin/udevd", "--daemon", NULL};
        execv("/sbin/udevd", udev_args);
        _exit(1);
    }
    sleep(1); 

    char *trigger_args[] = {"/sbin/udevadm", "trigger", "--action=add", NULL};
    run("/sbin/udevadm", trigger_args);

    char *settle_args[] = {"/sbin/udevadm", "settle", NULL};
    run("/sbin/udevadm", settle_args);
}

void setup_identity() {
    printf("** Remounting root RW and setting hostname... **\n");
    mount(NULL, "/", NULL, MS_REMOUNT, NULL);

    FILE *fp = fopen("/etc/HOSTNAME", "r");
    if (fp) {
        char name[64];
        if (fgets(name, sizeof(name), fp)) {
            name[strcspn(name, "\n")] = 0;
            sethostname(name, strlen(name));
        }
        fclose(fp);
    }
}

void setup_network() {
    printf("** Initializing Network... **\n");
    char *net_args[] = {"/etc/rc.d/rc.inet1", "start", NULL};
    run("/etc/rc.d/rc.inet1", net_args);
}

/* --- Main Entry --- */

int main() {
    if (getpid() != 1) return 1;

    /* Initial Environment */
    clearenv();
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin", 1);
    setenv("TERM", "linux", 1);

    /* Signal Handling */
    signal(SIGUSR1, poweroff);
    signal(SIGINT,  restart);
    reboot(RB_DISABLE_CAD);

    printf("\n** Sanity Standalone Booting **\n");

    /* Execution Flow */
    setup_api_filesystems();
    setup_hardware();           
    setup_terminal_subsystem(); 
    setup_identity();
    setup_network();

    printf("** Launching Supervisor Loop **\n");
    
    pid_t getty_pid = -1;

    while (1) {
        if (getty_pid <= 0) {
            getty_pid = fork();
            if (getty_pid == 0) {
                setsid();
                int fd = open("/dev/tty1", O_RDWR);
                if (fd >= 0) {
                    ioctl(fd, TIOCSCTTY, 1);
                    close(fd);
                }

                char *agetty_args[] = {"/sbin/agetty", "--noclear", "tty1", "115200", "linux", NULL};
                execv("/sbin/agetty", agetty_args);
                _exit(1);
            }
        }

        int status;
        pid_t reaped = wait(&status);

        if (reaped == getty_pid) {
            printf("** Getty exited. Respawning... **\n");
            getty_pid = -1;
            sleep(1); 
        } else {
            /* Cleanup any background noise/orphans */
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    return 0;
}
