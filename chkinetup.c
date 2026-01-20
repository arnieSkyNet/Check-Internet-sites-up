/*
 * chkinetup.c
 * 
 * Copyright (c) 2025 arnieSkyNet
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>
#include <getopt.h>
#include <stdarg.h>

#define PROGRAM "chkinetup"
#define VERSION "v0.09.03"
#define MAX_HOSTS 50
#define HOSTNAME_LEN 256
#define USERNAME_LEN 64
#define PATH_MAX_LEN 512
#define DEFAULT_CONNECT_TIMEOUT 2
#define MAX_BACKOFF_INTERVAL 60

volatile sig_atomic_t stop_program = 0;
FILE *log_file = NULL;
int interval = 5;
int debug = 0;
char username[USERNAME_LEN] = "";
char hostname[HOSTNAME_LEN] = "";

void handle_signal(int sig) {
    (void)sig;
    stop_program = 1;
}

void usage(FILE *stream) {
    fprintf(stream,
        "%s %s - Internet connectivity checker\n\n"
        "Usage: %s [delay] [options]\n\n"
        "Positional args:\n"
        "  delay                   Interval in seconds between checks (default: 5)\n\n"
        "Options:\n"
        "  -h, --help              Show this help message and exit\n"
        "  -d, --debug             Enable debug output and list hosts\n"
        "  -l, --logfile <name>    Set logfile name (default: <hostname>.log)\n"
        "  -L, --logdir <path>     Set logfile directory (default: $HOME/log)\n"
        "  -c, --checkfile <file>  File containing list of hosts to check (creates if missing)\n"
        "  -C, --clearfile         Ignore existing host file and regenerate\n"
        "  -H, --builtin-hosts     Use built-in host list\n"
        "  -v, --version           Show program version\n\n"
        "Written by ChatGPT vGPT-5-mini via guidance and design and massive corrections by ArnieSkyNet\n",
        PROGRAM, VERSION, PROGRAM
    );
}

void debugmsg(const char *fmt, ...) {
    if (!debug) return;
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    printf("[%02d-%02d-%04d %02d:%02d:%02d DEBUG] ",
           t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
           t->tm_hour, t->tm_min, t->tm_sec);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
}

void logmsg(const char *host, const char *msg) {
    if (!log_file) return;
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_file, "[%02d-%02d-%04d %02d:%02d:%02d %s %d %s] %s - %s\n",
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec,
            PROGRAM, interval, VERSION,
            host ? host : hostname, msg);
    fflush(log_file);

    if (debug) {
        printf("[%02d-%02d-%04d %02d:%02d:%02d LOG] %s - %s\n",
               t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
               t->tm_hour, t->tm_min, t->tm_sec,
               host ? host : hostname, msg);
        fflush(stdout);
    }
}

int check_host(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    int sock, rv, result;
    fd_set fdset;
    struct timeval tv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &res)) != 0) {
        debugmsg("getaddrinfo failed for %s: %s", host, gai_strerror(rv));
        return 0;
    }

    result = 0;
    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;

        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }

        connect(sock, p->ai_addr, p->ai_addrlen);

        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        tv.tv_sec = DEFAULT_CONNECT_TIMEOUT;
        tv.tv_usec = 0;

        if (select(sock + 1, NULL, &fdset, NULL, &tv) > 0) {
            int so_error;
            socklen_t len = sizeof so_error;
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
            if (so_error == 0) result = 1;
        }

        close(sock);
        if (result) break;
    }

    freeaddrinfo(res);
    return result;
}

time_t get_mtime(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_mtime;
    return 0;
}

int create_dir(const char *path) {
    char *tmp = strdup(path);
    if (!tmp) return -1;
    
    char *dir = dirname(tmp);
    int result = mkdir(dir, 0755);
    
    free(tmp);
    
    if (result == 0 || errno == EEXIST)
        return 0;
    
    return -1;
}

void cleanup_hosts(char *hosts[], int num_hosts) {
    for (int i = 0; i < num_hosts; i++) {
        if (hosts[i]) {
            free(hosts[i]);
            hosts[i] = NULL;
        }
    }
}

int load_hosts_from_file(const char *filename, char *hosts[], int max_hosts, 
                         const char *builtin_hosts[], int builtin_count) {
    int num_hosts = 0;
    FILE *f = fopen(filename, "r");
    
    if (!f) {
        create_dir(filename);
        
        f = fopen(filename, "w");
        if (!f) {
            debugmsg("Failed to create host file: %s", filename);
            return 0;
        }
        
        for (int i = 0; i < builtin_count && i < max_hosts; i++) {
            fprintf(f, "%s\n", builtin_hosts[i]);
            char *dup = strdup(builtin_hosts[i]);
            if (!dup) {
                debugmsg("strdup failed for host: %s", builtin_hosts[i]);
                break;
            }
            hosts[num_hosts++] = dup;
        }
        fclose(f);
        debugmsg("Created new host file with %d built-in hosts", num_hosts);
    } else {
        char line[HOSTNAME_LEN];
        while (fgets(line, sizeof(line), f) && num_hosts < max_hosts) {
            line[strcspn(line, "\r\n")] = 0;
            
            if (line[0] == '#' || strlen(line) == 0)
                continue;
            
            if (strlen(line) >= HOSTNAME_LEN) {
                debugmsg("Hostname too long, skipping: %.50s...", line);
                continue;
            }
            
            char *dup = strdup(line);
            if (!dup) {
                debugmsg("strdup failed for host: %s", line);
                break;
            }
            hosts[num_hosts++] = dup;
        }
        fclose(f);
        debugmsg("Loaded %d hosts from file", num_hosts);
    }
    
    return num_hosts;
}

int validate_logfile_name(const char *name) {
    if (!name) return 0;
    
    if (strchr(name, '/') || strchr(name, '\\')) {
        fprintf(stderr, "Error: Logfile name cannot contain path separators\n");
        return 0;
    }
    
    return 1;
}

int main(int argc, char *argv[]) {
    const char *default_hosts[] = {
        "www.google.com",
        "www.cloudflare.com",
        "www.microsoft.com",
        "www.amazon.com",
        "www.bbc.co.uk"
    };
    const int default_hosts_count = sizeof(default_hosts) / sizeof(default_hosts[0]);
    
    char *hosts[MAX_HOSTS] = {0};
    int num_hosts = 0;
    int state[MAX_HOSTS];
    time_t checkfile_mtime = 0;

    char *logfile_name = NULL, *logdir = NULL, *checkfile = NULL;
    int use_builtin_hosts = 0, clear_host_file = 0;

    static struct option long_opts[] = {
        {"help", no_argument, 0, 'h'},
        {"debug", no_argument, 0, 'd'},
        {"logfile", required_argument, 0, 'l'},
        {"logdir", required_argument, 0, 'L'},
        {"checkfile", required_argument, 0, 'c'},
        {"builtin-hosts", no_argument, 0, 'H'},
        {"clearfile", no_argument, 0, 'C'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hdl:L:c:CHv", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'h': 
                usage(stdout); 
                return 0;
            case 'd': 
                debug = 1; 
                break;
            case 'l': 
                logfile_name = strdup(optarg);
                if (!logfile_name) {
                    perror("strdup failed");
                    return 1;
                }
                if (!validate_logfile_name(logfile_name)) {
                    free(logfile_name);
                    return 1;
                }
                break;
            case 'L': 
                logdir = strdup(optarg);
                if (!logdir) {
                    perror("strdup failed");
                    return 1;
                }
                break;
            case 'c': 
                checkfile = strdup(optarg);
                if (!checkfile) {
                    perror("strdup failed");
                    return 1;
                }
                break;
            case 'H': 
                use_builtin_hosts = 1; 
                break;
            case 'C': 
                clear_host_file = 1; 
                break;
            case 'v': 
                printf("%s %s\n", PROGRAM, VERSION); 
                return 0;
            default: 
                usage(stderr); 
                return 1;
        }
    }

    if (optind < argc) {
        int val = atoi(argv[optind]);
        if (val > 0) interval = val;
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    }
    if (gethostname(hostname, sizeof(hostname) - 1) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
    }
    hostname[sizeof(hostname) - 1] = '\0';

    if (clear_host_file && checkfile) {
        if (unlink(checkfile) == 0 || errno == ENOENT) {
            debugmsg("Deleted host file: %s", checkfile);
        } else {
            perror("Failed to remove host file");
        }
    }

    if (checkfile && !use_builtin_hosts) {
        num_hosts = load_hosts_from_file(checkfile, hosts, MAX_HOSTS, 
                                         default_hosts, default_hosts_count);
    } else {
        for (int i = 0; i < default_hosts_count && i < MAX_HOSTS; i++) {
            char *dup = strdup(default_hosts[i]);
            if (!dup) {
                fprintf(stderr, "strdup failed for built-in host\n");
                cleanup_hosts(hosts, num_hosts);
                return 1;
            }
            hosts[num_hosts++] = dup;
        }
    }

    if (num_hosts == 0) {
        fprintf(stderr, "Error: No hosts to check\n");
        return 1;
    }

    for (int i = 0; i < num_hosts; i++) {
        state[i] = -1;
    }

    char logfile_path[PATH_MAX_LEN];
    const char *home = logdir ? logdir : getenv("HOME");
    if (!home) home = "/tmp";

    int n;
    if (logfile_name) {
        n = snprintf(logfile_path, sizeof(logfile_path), "%s/log/%s", home, logfile_name);
    } else {
        n = snprintf(logfile_path, sizeof(logfile_path), "%s/log/%s.log", home, hostname);
    }

    if (n >= (int)sizeof(logfile_path)) {
        fprintf(stderr, "Error: Log path too long\n");
        cleanup_hosts(hosts, num_hosts);
        if (logfile_name) free(logfile_name);
        if (logdir) free(logdir);
        if (checkfile) free(checkfile);
        return 1;
    }

    if (create_dir(logfile_path) != 0) {
        fprintf(stderr, "Warning: Failed to create log directory\n");
    }

    log_file = fopen(logfile_path, "a");
    if (!log_file) {
        perror("fopen logfile");
        cleanup_hosts(hosts, num_hosts);
        if (logfile_name) free(logfile_name);
        if (logdir) free(logdir);
        if (checkfile) free(checkfile);
        return 1;
    }

    if (debug) {
        usage(stdout);
        printf("\nConfiguration:\n");
        printf("  Interval: %d seconds\n", interval);
        printf("  Log file: %s\n", logfile_path);
        printf("\nHosts list in use:\n");
        for (int i = 0; i < num_hosts; i++) {
            printf("  %s\n", hosts[i]);
        }
        printf("\n");
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    logmsg(NULL, "started");

    if (checkfile) {
        checkfile_mtime = get_mtime(checkfile);
    }

    int global_connected = 1;
    
    while (!stop_program) {
        int up = 0;
        char restored_host[HOSTNAME_LEN] = "";

        if (checkfile) {
            time_t new_mtime = get_mtime(checkfile);
            if (new_mtime > checkfile_mtime) {
                logmsg(NULL, "Host file changed, reloading");

                cleanup_hosts(hosts, num_hosts);
                num_hosts = 0;

                num_hosts = load_hosts_from_file(checkfile, hosts, MAX_HOSTS,
                                                 default_hosts, default_hosts_count);

                for (int i = 0; i < num_hosts; i++) {
                    state[i] = -1;
                }

                checkfile_mtime = new_mtime;

                for (int i = 0; i < num_hosts; i++) {
                    logmsg(hosts[i], "loaded from host file");
                }

                if (debug) {
                    printf("Reloaded hosts list:\n");
                    for (int i = 0; i < num_hosts; i++) {
                        printf("  %s\n", hosts[i]);
                    }
                }
            }
        }

        for (int i = 0; i < num_hosts; i++) {
            int ok = check_host(hosts[i], "443");
            
            if (ok) {
                if (state[i] == 0) {
                    logmsg(hosts[i], "connectivity restored");
                }
                state[i] = 1;
                
                if (!global_connected && restored_host[0] == '\0') {
                    strncpy(restored_host, hosts[i], sizeof(restored_host) - 1);
                    restored_host[sizeof(restored_host) - 1] = '\0';
                }
                up = 1;
            } else {
                if (state[i] != 0) {
                    logmsg(hosts[i], "unreachable");
                }
                state[i] = 0;
            }
        }

        if (up && !global_connected) {
            logmsg(restored_host, "Global connectivity restored");
            global_connected = 1;
        }
        if (!up && global_connected) {
            logmsg(NULL, "All hosts unreachable");
            global_connected = 0;
        }

        int sleep_interval = interval;
        if (!global_connected) {
            sleep_interval = interval * 2;
            if (sleep_interval > MAX_BACKOFF_INTERVAL) {
                sleep_interval = MAX_BACKOFF_INTERVAL;
            }
            debugmsg("Using backoff interval: %d seconds", sleep_interval);
        }
        
        sleep(sleep_interval);
    }

    logmsg(NULL, "stopped");
    
    if (log_file) {
        fclose(log_file);
    }
    
    cleanup_hosts(hosts, num_hosts);
    
    if (logfile_name) free(logfile_name);
    if (logdir) free(logdir);
    if (checkfile) free(checkfile);

    return 0;
}

