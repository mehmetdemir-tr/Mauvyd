#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) { 
        printf("Usage: mauvyctl <status|stop> [service_name]\n"); 
        return 1; 
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/run/mauvyd.sock", sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Cannot connect to the system, is Mauvyd running?");
        close(fd);
        return 1;
    }

    char cmd[256] = {0};
    snprintf(cmd, sizeof(cmd), "%s", argv[1]);
    if (argc > 2) {
        strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, argv[2], sizeof(cmd) - strlen(cmd) - 1);
    }
    
    write(fd, cmd, strlen(cmd));

    char buf[4096];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, n);
    }

    close(fd);
    return 0;
}
