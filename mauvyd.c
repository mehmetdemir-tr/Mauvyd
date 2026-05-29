#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h> 

#include "mauvyd-headers/mauvyd.h"
#include "mauvyd-headers/pcg.h"

#define CONFIG_DIR "/etc/mauvyd"
#define DEFAULT_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define HOSTNAME "local@tuxshome" 
#define OS_NAME "An Linux Distro" 

Service service_table[MAX_SERVICES];
int service_count = 0;
sigset_t block_mask, old_mask;

void setup_network(void);
void sigchld_handler(int sig);
void* start_control_socket(void* arg);
int is_running(const char *name);
void spawn_service(int idx);
void resolve_and_start(int idx);
void start_services(struct dirent **namelist, int n);

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

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t dead_pid;

    while ((dead_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < service_count; i++) {
            if (service_table[i].pid == dead_pid && service_table[i].restart) {
                sleep(1); 

                pid_t new_pid = fork();
                if (new_pid == 0) {
                    char *argv[32] = {0};
                    argv[0] = service_table[i].location;
                    char tmp[512];
                    strncpy(tmp, service_table[i].args_str, 511);
                    char *token = strtok(tmp, " ");
                    int j = 1;
                    while (token && j < 31) { argv[j++] = token; token = strtok(NULL, " "); }
                    execv(service_table[i].location, argv);
                    exit(1);
                }
                service_table[i].pid = new_pid;
            }
        }
    }
}

void* start_control_socket(void* arg) {
    (void)arg;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/run/mauvyd.sock", sizeof(addr.sun_path) - 1);

    unlink("/run/mauvyd.sock");
    mkdir("/run", 0755);
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return NULL;
    }
    listen(fd, 5);

    while (1) {
        int client = accept(fd, NULL, NULL);
        if (client < 0) continue;

        char buf[256] = {0};
        read(client, buf, sizeof(buf) - 1);

        if (strncmp(buf, "status", 6) == 0) {
            for (int i = 0; i < service_count; i++) {
                char line[512];
                int alive = (kill(service_table[i].pid, 0) == 0);
                snprintf(line, sizeof(line), "%s: %s (pid %d)\n",
                    service_table[i].location,
                    alive ? "running" : "dead",
                    service_table[i].pid);
                write(client, line, strlen(line));
            }
        } else if (strncmp(buf, "stop ", 5) == 0) {
            char *name = buf + 5;
            int found = 0;
            for (int i = 0; i < service_count; i++) {
                if (strstr(service_table[i].location, name)) {
                    kill(service_table[i].pid, SIGTERM);
                    write(client, "Stopping service...\n", 20);
                    found = 1;
                }
            }
            if (!found) write(client, "Service not found.\n", 19);
        }

        close(client);
    }
    return NULL;
}

void start_services(struct dirent **namelist, int n) {
    for (int i = 0; i < n; i++) {
        struct dirent *entry = namelist[i];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            free(namelist[i]);
            continue;
        }

        char service_name[64] = {0};
        strncpy(service_name, entry->d_name, sizeof(service_name) - 1);
        char *dot = strrchr(service_name, '.');
        if (dot) *dot = '\0'; 

        char fulldst[512];
        snprintf(fulldst, sizeof(fulldst), "%s/%s", CONFIG_DIR, entry->d_name);

        char location[256] = {0};
        char wait_str[16] = {0};
        char interactive_str[16] = {0};
        char args_str[512] = {0};
        char depends_str[256] = {0};
        char restart_str[16] = {0};

        pcg_read(fulldst, "location", location, sizeof(location));
        pcg_read(fulldst, "interactive", interactive_str, sizeof(interactive_str));
        pcg_read(fulldst, "wait", wait_str, sizeof(wait_str));
        pcg_read(fulldst, "depends", depends_str, sizeof(depends_str));
        pcg_read(fulldst, "args", args_str, sizeof(args_str));
        pcg_read(fulldst, "restart", restart_str, sizeof(restart_str));

        if (location[0] != '/') {
            free(namelist[i]);
            continue;
        }

        if (service_count >= MAX_SERVICES) {
            free(namelist[i]);
            continue;
        }

        service_table[service_count].pid = 0;
        strncpy(service_table[service_count].name, service_name, 63);
        strncpy(service_table[service_count].location, location, 255);
        strncpy(service_table[service_count].args_str, args_str, 511);
        strncpy(service_table[service_count].depends_str, depends_str, 255);
        service_table[service_count].restart = (strcmp(restart_str, "1") == 0);
        service_table[service_count].visit_state = STATE_UNVISITED;
        service_count++;

        free(namelist[i]);
    }

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);

    for (int i = 0; i < service_count; i++) {
        if (service_table[i].visit_state == STATE_UNVISITED) {
            resolve_and_start(i);
        }
    }
}

void spawn_service(int idx) {
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    pid_t pid = fork();
    if (pid == 0) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        char fulldst[512];
        snprintf(fulldst, sizeof(fulldst), "%s/%s.pcg", CONFIG_DIR, service_table[idx].name);
        char interactive_str[16] = {0};
        pcg_read(fulldst, "interactive", interactive_str, sizeof(interactive_str));

        if (strcmp(interactive_str, "1") == 0) {
            int console_fd = open("/dev/console", O_RDWR);
            if (console_fd >= 0) {
                dup2(console_fd, STDIN_FILENO);
                dup2(console_fd, STDOUT_FILENO);
                dup2(console_fd, STDERR_FILENO);
                close(console_fd);
            }
        } else {
            char logpath[256];
            snprintf(logpath, sizeof(logpath), "/var/log/mauvyd/%s.log", service_table[idx].name);
            mkdir("/var/log/mauvyd", 0755);
            int logfd = open(logpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (logfd >= 0) {
                dup2(logfd, STDOUT_FILENO);
                dup2(logfd, STDERR_FILENO);
                close(logfd);
            }
        }

        char *argv[32] = {0};
        argv[0] = service_table[idx].location;
        if (service_table[idx].args_str[0] != '\0') {
            char *token = strtok(service_table[idx].args_str, " ");
            int j = 1;
            while (token && j < 31) {
                argv[j++] = token;
                token = strtok(NULL, " ");
            }
        }

        execv(service_table[idx].location, argv);
        perror("execv failed");
        exit(1);
    } else if (pid > 0) {
        service_table[idx].pid = pid;
        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        char fulldst[512];
        snprintf(fulldst, sizeof(fulldst), "%s/%s.pcg", CONFIG_DIR, service_table[idx].name);
        char wait_str[16] = {0};
        pcg_read(fulldst, "wait", wait_str, sizeof(wait_str));
        
        if (strcmp(wait_str, "1") == 0) {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

int is_running(const char *name) {
    for (int i = 0; i < service_count; i++) {
        if (strcmp(service_table[i].name, name) == 0) {
            if (service_table[i].pid <= 0) return 0;
            return (kill(service_table[i].pid, 0) == 0);
        }
    }
    return 0;
}

void resolve_and_start(int idx) {
    if (service_table[idx].visit_state == STATE_VISITING) {
        printf("[!!!] Circular dependency detected at service: %s! Breaking loop.\n", service_table[idx].name);
        return;
    }
    if (service_table[idx].visit_state == STATE_VISITED) {
        return;
    }

    service_table[idx].visit_state = STATE_VISITING;

    if (service_table[idx].depends_str[0] != '\0') {
        int dep_found = -1;
        for (int i = 0; i < service_count; i++) {
            if (strcmp(service_table[i].name, service_table[idx].depends_str) == 0) {
                dep_found = i;
                break;
            }
        }

        if (dep_found != -1) {
            resolve_and_start(dep_found);
            
            int retries = 0;
            while (!is_running(service_table[idx].depends_str) && retries < 20) {
                usleep(50000);
                retries++;
            }
        } else {
            printf("[!!!] Warning: Dependency '%s' for '%s' not found in configs!\n", 
                   service_table[idx].depends_str, service_table[idx].name);
        }
    }

    printf("[+]: Starting service: %s\n", service_table[idx].name);
    spawn_service(idx);

    service_table[idx].visit_state = STATE_VISITED;
}

int main() {
    struct dirent **namelist;
    int n = scandir(CONFIG_DIR, &namelist, NULL, alphasort);
    
    printf("----------------------------------------\n");
    printf("----- MAUVYD Configuration System -----\n");
    printf("-------------Mounting FS..--------------\n");

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    
    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", 0, "gid=5,mode=620");
    mkdir("/data", 0755);

    int ret = mount("/dev/vda", "/data", "ext4", 0, NULL); 
    if (ret == 0) printf("[+]: Persistence is active.\n");

    sethostname(HOSTNAME, strlen(HOSTNAME));
    setenv("PATH", DEFAULT_PATH, 1);
    setenv("TERM", "linux", 1);
    printf("%s\n", OS_NAME);

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    setup_network();

    pthread_t socket_thread;
    if (pthread_create(&socket_thread, NULL, start_control_socket, NULL) == 0) {
        pthread_detach(socket_thread);
        printf("[+]: Control socket thread initialized.\n");
    }

    if (n >= 0) {
        start_services(namelist, n);
        free(namelist);
    }

    while(1) {
        pid_t shell_pid = fork();
        if (shell_pid == 0) {
            int console_fd = open("/dev/console", O_RDWR);
            if (console_fd >= 0) {
                dup2(console_fd, STDIN_FILENO);
                dup2(console_fd, STDOUT_FILENO);
                dup2(console_fd, STDERR_FILENO);
                close(console_fd);
            }
            char *shell_args[] = {"/bin/sh", NULL};
            execv("/bin/sh", shell_args);
            exit(1);
        } else if (shell_pid > 0) {
            int status;
            waitpid(shell_pid, &status, 0);
            sleep(1);
        } else {
            sleep(2);
        }
    }
    return 0;
}
