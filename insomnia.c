#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define SANITY_DIR "/etc/gni.d/"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: insomnia [reboot|off|die|enable <script>|disable <script>]\n");
        return 1;
    }

    // Handle enable/disable (File System logic)
    if (argc == 3) {
        char path[256];
        snprintf(path, sizeof(path), "%s%s", SANITY_DIR, argv[2]);

        struct stat st;
        if (stat(path, &st) != 0) {
            perror("** Script not found **");
            return 1;
        }

        if (strcmp(argv[1], "enable") == 0) {
            chmod(path, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
            printf("** Enabled %s **\n", argv[2]);
            return 0;
        } 
        else if (strcmp(argv[1], "disable") == 0) {
            chmod(path, st.st_mode & ~(S_IXUSR | S_IXGRP | S_IXOTH));
            printf("** Disabled %s **\n", argv[2]);
            return 0;
        }
    }

    // Handle signals to Sanity (FIFO logic)
    int fd = open("/run/gni.fifo", O_WRONLY);
    if (fd < 0) {
        perror("** Cannot reach GNI (is the FIFO there?) **");
        return 1;
    }

    if (strcmp(argv[1], "kill") == 0 || strcmp(argv[1], "die") == 0) {
        write(fd, "die", 3);
        printf("** Sent termination signal to PID 1. **\n ");
    } else {
        write(fd, argv[1], strlen(argv[1]));
        printf("** Sent '%s' **\n", argv[1]);
    }

    close(fd);
    return 0;
}
