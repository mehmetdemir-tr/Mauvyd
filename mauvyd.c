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

int main() {
    struct dirent *entry;
    pid_t pid;
    struct dirent **namelist;
    int n = scandir("path/to/config/folder", &namelist, NULL, alphasort);
    if (n < 0) {
        perror("[!]: Can't open the config folder, Continue to boot..");
        char *shell_args[] = {"/path/to/shell", NULL};
        execv("/bin/shell", shell_args);
        while(wait(NULL) > 0);
        return 0;
    }
    printf("----------------------------------------\n");
    printf("----- MAUVYD Configuration System -----\n");
    printf("-------------Mounting FS..--------------\n");
    mount("proc", "/proc", "proc", 0, NULL); // Start of the Mount and the Persistence settings
    mount("sysfs", "/sys", "sysfs", 0, NULL); // You can change these how as you want it! :)
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    mkdir("/data", 0755);
    mkdir("/data/...", 0755); // <-- change here (example: /data/<yourcommandfolder)
    int ret = mount("/dev/vda", "/data", "ext4", 0, NULL); // <-- you can add more mount points and device nodes much as you want.
    if (ret == 0) {
        printf("[+]: Persistence is active.\n");
    } else if (errno == EBUSY) {
        printf("[?]: Persistence got activated already.\n");
    } else {
        perror("[!]: Can't connect the disk (persistence)\n");
    }
    printf("[!] Setting up the hostname..\n");
    sethostname("hostname_here", strlen("hostname_here")); // <-- change here
    putenv("PATH=path/to/commands/folder");
    putenv("TERM=linux");
    printf("Your OS Name here (f.e PatiOS by PatiOS Team)\n");

    struct ifreq ifr;
    int tmp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "lo", IFNAMSIZ);
    ioctl(tmp_fd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl(tmp_fd, SIOCSIFFLAGS, &ifr);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    ioctl(tmp_fd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl(tmp_fd, SIOCSIFFLAGS, &ifr);

    close(tmp_fd);

for (int i = 0; i < n; i++) {
    entry = namelist[i];
    if (strcmp(entry->d_name, ".") == 0) {
        continue;
    };
    if (strcmp(entry->d_name, "..") == 0) {
        continue;
    };
      usleep(10000);
      printf("Found Process: %s\n", entry->d_name);
      char fulldst[512];
      snprintf(fulldst, sizeof(fulldst), "/path/to/config/folder/%s", entry->d_name); // <-- Change here
    char filedst[256] = {0};
    char wait_val[16] = {0};
    char watch_val[16] = {0};
    pcg_read(fulldst, "location", filedst, sizeof(filedst));
    if (filedst[0] != '/' || strstr(filedst, "..") != NULL) {
        printf("[!!!] Skipping this, wrong folder location: %s\n", filedst);
        free(namelist[i]);
        continue;
    }
    pcg_read(fulldst, "wait", wait_val, sizeof(wait_val));
    pcg_read(fulldst, "watch", watch_val, sizeof(watch_val));
    char *args[] = {NULL, NULL};
    args[0] = filedst;
    int wait_flag = (strcmp(wait_val, "1") == 0);
    int watch_flag = (strcmp(watch_val, "1") == 0);


      pid = fork(); // FORK TIME!

    if (pid == -1) {
        perror("Fork failed.");
        exit(EXIT_FAILURE);
        }
    if (pid > 0 && wait_flag == 1) {
        usleep(100000);
        waitpid(pid, NULL, 0);
        }

    if (pid == 0) {
        printf("Child Process starting this service: %s\n", filedst);
        execv(filedst, args);
        perror("Oops, process got sick! (cant run this service)");
        exit(EXIT_FAILURE);
        }
free(namelist[i]);
}
free(namelist);
signal(SIGCHLD, SIG_IGN);
sigset_t mask;
sigemptyset(&mask);
sigsuspend(&mask);
}
