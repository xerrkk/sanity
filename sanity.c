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

#define FIFO_PATH "/run/sanity.fifo"

/* --- System Control --- */

void kill_the_world() {
    printf("\n** The system is going down NOW... **\n");
    // Send TERM to everyone but us
    kill(-1, SIGTERM);
    sleep(2);
    // Send KILL to the survivors
    kill(-1, SIGKILL);
    sync();
}

void panic() {
    printf("\n** The system is going down NOW... **\n");
    exit(42); // Kernel will panic immediately when PID 1 exits
}

void poweroff() {
    kill_the_world();
    reboot(RB_POWER_OFF);
}

void restart() {
    kill_the_world();
    reboot(RB_AUTOBOOT);
}

/* --- Core Logic --- */

void run(char *cmd, char *args[]) {
    if (access(cmd, X_OK) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        execv(cmd, args);
        _exit(1);
    }
    // Note: This remains blocking for boot-time setup scripts
    waitpid(pid, NULL, 0); 
}

void setup_api_filesystems() {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/run", 0755);
    mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=32M");
}

/* --- The Listener --- */

void handle_insomnia(int fd) {
    char buf[64];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    if (strcmp(buf, "die") == 0) {
        kill_the_world();
        printf("** System halted. **\n");
        while(1) pause(); // Stay alive so kernel doesn't panic unless asked
    } 
    else if (strcmp(buf, "panic") == 0) {
        panic();
    }
    else if (strcmp(buf, "reboot") == 0) {
        restart();
    }
    else if (strcmp(buf, "off") == 0) {
        poweroff();
    }
}

int main() {
    if (getpid() != 1) return 1;

    /* Environment Setup */
    clearenv();
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin", 1);
    
    setup_api_filesystems();
    
    /* Create Command Pipe */
    unlink(FIFO_PATH);
    mkfifo(FIFO_PATH, 0600);
    int fifo_fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);

    printf("\n** The system is going UP now... **\n");

    /* Main Supervisor Loop */
    while (1) {
        handle_insomnia(fifo_fd);

        // Reap ANY orphaned children (The Reaper)
        while (waitpid(-1, NULL, WNOHANG) > 0);

        usleep(50000); // 50ms pulse
    }

    return 0;
}
