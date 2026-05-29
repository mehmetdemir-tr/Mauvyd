#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include "mauvyd-headers/pcg.h"

#define CONFIG_DIR "/etc/mauvyd"
#define DEFAULT_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define HOSTNAME "local@tuxshome" // <-- you can change this
#define OS_NAME "An Linux Distro" // <-- you can change this too

void setup_network() {
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return; }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "lo", IFNAMSIZ);
    ioctl(fd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl(fd, SIOCSIFFLAGS, &ifr);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "ens3", IFNAMSIZ);
    ioctl(fd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl(fd, SIOCSIFFLAGS, &ifr);

    close(fd);
    printf("[+]: Network interfaces activated.\n");
}

void start_services(struct dirent **namelist, int n) {
    for (int i = 0; i < n; i++) {
        struct dirent *entry = namelist[i];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            free(namelist[i]);
            continue;
        }

        char fulldst[512];
        snprintf(fulldst, sizeof(fulldst), "%s/%s", CONFIG_DIR, entry->d_name);

        char location[256] = {0};
        char wait_str[16] = {0};
        char args_str[512] = {0};

        pcg_read(fulldst, "location", location, sizeof(location));
        pcg_read(fulldst, "wait", wait_str, sizeof(wait_str));
        pcg_read(fulldst, "args", args_str, sizeof(args_str));

        if (location[0] != '/') {
            printf("[!!!] Invalid location: %s\n", entry->d_name);
            free(namelist[i]);
            continue;
        }

        printf("Found Process: %s\n", entry->d_name);

        pid_t pid = fork();
        if (pid == 0) {
            printf("Child Process starting this service: %s\n", location);

            char *argv[32] = {0};
            argv[0] = location;

            if (args_str[0] != '\0') {
                char *token = strtok(args_str, " ");
                int j = 1;
                while (token && j < 31) {
                    argv[j++] = token;
                    token = strtok(NULL, " ");
                }
            }

            execv(location, argv);
            perror("execv failed");
            exit(1);
        } else if (pid > 0 && strcmp(wait_str, "1") == 0) {
            waitpid(pid, NULL, 0);
        }

        free(namelist[i]);
    }
}

int main() {
    pid_t pid;
    struct dirent **namelist;
    int n = scandir(CONFIG_DIR, &namelist, NULL, alphasort);
    if (n < 0) {
        perror("[!]: Can't open the config folder, Continue to boot..");
        char *shell_args[] = {"/bin/sh", NULL};
        execv("/bin/sh", shell_args);
        while(wait(NULL) > 0);
        return 0;
    }

    printf("----------------------------------------\n");
    printf("----- MAUVYD Configuration System -----\n");
    printf("-------------Mounting FS..--------------\n");

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    mkdir("/data", 0755);

    int ret = mount("/dev/vda", "/data", "ext4", 0, NULL); // <-- you can add more mount points and device nodes as you want
    if (ret == 0) {
        printf("[+]: Persistence is active.\n");
    } else if (errno == EBUSY) {
        printf("[?]: Persistence got activated already.\n");
    } else {
        perror("[!]: Can't connect the disk (persistence)\n");
    }

    printf("[!] Setting up the hostname..\n");
    sethostname(HOSTNAME, strlen(HOSTNAME));
    setenv("PATH", DEFAULT_PATH, 1);
    setenv("TERM", "linux", 1);
    printf("%s\n", OS_NAME);

    signal(SIGCHLD, SIG_IGN);
    setup_network();
    start_services(namelist, n);
    free(namelist);

    while(1) pause();
    return 0;
}
