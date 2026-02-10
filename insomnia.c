#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: insomnia [reboot|off|die]\n");
        return 1;
    }

    int fd = open("/run/sanity.fifo", O_WRONLY);
    if (fd < 0) {
        perror("Insomnia: Cannot reach Sanity (is the FIFO there?)");
        return 1;
    }

    if (strcmp(argv[1], "kill") == 0 || strcmp(argv[1], "die") == 0) {
        write(fd, "die", 3);
        printf("Insomnia: Sent termination signal to PID 1.\n");
    } else {
        write(fd, argv[1], strlen(argv[1]));
    }

    close(fd);
    return 0;
}
