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

#define PROGRAM "chkinetup"
#define VERSION "v0.09.02"
#define MAX_HOSTS 50
#define HOSTNAME_LEN 128
#define USERNAME_LEN 64

volatile sig_atomic_t stop_program = 0;
FILE *log_file = NULL;
int interval = 5;
int debug = 0;
char username[USERNAME_LEN] = "";
char hostname[HOSTNAME_LEN] = "";

// Signal handler
void handle_signal(int sig) {
    stop_program = 1;
}

// Usage/help
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

// Logging
void logmsg(const char *host, const char *msg) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_file, "[%02d:%02d:%04d %02d:%02d:%02d %s %d %s] %s - %s\n",
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec,
            PROGRAM, interval, VERSION,
            host ? host : hostname, msg);
    fflush(log_file);

    if (debug) {
        printf("[%02d:%02d:%04d %02d:%02d:%02d DEBUG] %s - %s\n",
               t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
               t->tm_hour, t->tm_min, t->tm_sec,
               host ? host : hostname, msg);
        fflush(stdout);
    }
}

// TCP connect with timeout
int check_host(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    int sock, rv, result;
    fd_set fdset;
    struct timeval tv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &res)) != 0) {
        if (debug) printf("getaddrinfo failed for %s: %s\n", host, gai_strerror(rv));
        return 0;
    }

    result = 0;
    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        connect(sock, p->ai_addr, p->ai_addrlen);

        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        tv.tv_sec = 2;
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

// Get modification time of a file
time_t get_mtime(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_mtime;
    return 0;
}

// Load or create hosts file
int load_hosts_from_file(const char *filename, char *hosts[], int max_hosts, const char *builtin_hosts[], int builtin_count) {
    int num_hosts = 0;
    FILE *f = fopen(filename, "r");
    if (!f) {
        // Create folder and file
        char tmp[512];
        strncpy(tmp, filename, sizeof(tmp)-1);
        tmp[sizeof(tmp)-1]='\0';
        mkdir(dirname(tmp), 0755);
        f = fopen(filename, "w");
        if (!f) return 0;
        for (int i = 0; i < builtin_count && i < max_hosts; i++) {
            fprintf(f, "%s\n", builtin_hosts[i]);
            hosts[num_hosts++] = strdup(builtin_hosts[i]);
        }
        fclose(f);
    } else {
        char line[256];
        while (fgets(line, sizeof(line), f) && num_hosts < max_hosts) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] != '#' && strlen(line) > 0)
                hosts[num_hosts++] = strdup(line);
        }
        fclose(f);
    }
    return num_hosts;
}

int main(int argc, char *argv[]) {
    const char *default_hosts[] = {"www.google.com","www.cloudflare.com","www.microsoft.com","www.amazon.com","www.bbc.co.uk"};
    char *hosts[MAX_HOSTS];
    int num_hosts = 0;
    int state[MAX_HOSTS];
    time_t checkfile_mtime = 0;

    char *logfile_name=NULL, *logdir=NULL, *checkfile=NULL;
    int use_builtin_hosts=0, clear_host_file=0;

    static struct option long_opts[] = {
        {"help", no_argument,0,'h'},
        {"debug", no_argument,0,'d'},
        {"logfile", required_argument,0,'l'},
        {"logdir", required_argument,0,'L'},
        {"checkfile", required_argument,0,'c'},
        {"builtin-hosts", no_argument,0,'H'},
        {"clearfile", no_argument,0,'C'},
        {"version", no_argument,0,'v'},
        {0,0,0,0}
    };

    int opt;
    while((opt=getopt_long(argc,argv,"hdl:L:c:CHv",long_opts,NULL))!=-1){
        switch(opt){
            case 'h': usage(stdout); return 0;
            case 'd': debug=1; break;
            case 'l': logfile_name=strdup(optarg); break;
            case 'L': logdir=strdup(optarg); break;
            case 'c': checkfile=strdup(optarg); break;
            case 'H': use_builtin_hosts=1; break;
            case 'C': clear_host_file=1; break;
            case 'v': printf("%s %s\n", PROGRAM, VERSION); return 0;
            default: usage(stderr); return 1;
        }
    }

    // Positional delay arg
    if(optind<argc){
        int val=atoi(argv[optind]);
        if(val>0) interval=val;
    }

    // username/hostname
    struct passwd *pw=getpwuid(getuid());
    if(pw) strncpy(username,pw->pw_name,sizeof(username)-1);
    username[sizeof(username)-1]='\0';
    gethostname(hostname,sizeof(hostname)-1);
    hostname[sizeof(hostname)-1]='\0';

    // Handle -C option: clear host file if requested
    if (clear_host_file && checkfile) {
        if (access(checkfile, F_OK) == 0) {
            if (unlink(checkfile) == 0) {
                logmsg(NULL, "Existing host file removed due to -C option");
                if (debug) printf("Deleted host file: %s\n", checkfile);
            } else {
                perror("Failed to remove host file");
            }
        }
    }

    // Load hosts
    if(checkfile && !use_builtin_hosts)
        num_hosts = load_hosts_from_file(checkfile, hosts, MAX_HOSTS, default_hosts, 5);
    else {
        for(int i=0;i<5;i++) hosts[num_hosts++]=strdup(default_hosts[i]);
    }

    for(int i=0;i<num_hosts;i++) state[i]=-1;

    // Logfile
    char logfile_path[512];
    if(!logdir) logdir=strdup(getenv("HOME"));
    if(logfile_name) snprintf(logfile_path,sizeof(logfile_path),"%s/log/%s",logdir,logfile_name);
    else snprintf(logfile_path,sizeof(logfile_path),"%s/log/%s.log",logdir,hostname);

    char tmp[512]; strncpy(tmp,logfile_path,sizeof(tmp)-1); tmp[sizeof(tmp)-1]='\0';
    mkdir(dirname(tmp),0755);

    log_file=fopen(logfile_path,"a");
    if(!log_file){ perror("fopen logfile"); return 1; }

    if(debug){
        usage(stdout);
        printf("Hosts list in use:\n");
        for(int i=0;i<num_hosts;i++) printf("  %s\n",hosts[i]);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    logmsg(NULL,"started");

    if(checkfile)
        checkfile_mtime = get_mtime(checkfile);

    int global_connected=1;
    while(!stop_program){
        int up=0;
        char restored_host[HOSTNAME_LEN]="";

// Auto-reload host file if modified
if (checkfile) {
    time_t new_mtime = get_mtime(checkfile);
    if (new_mtime > checkfile_mtime) {
        logmsg(NULL, "Host file changed, reloading");

        // Clear existing hosts
        for (int i = 0; i < num_hosts; i++) {
            free(hosts[i]);
        }

        num_hosts = load_hosts_from_file(checkfile, hosts, MAX_HOSTS,
                                         default_hosts, 5);

        // Reset states
        for (int i = 0; i < num_hosts; i++) {
            state[i] = -1;
        }

        checkfile_mtime = new_mtime;

        // Log every host that is now active
        for (int i = 0; i < num_hosts; i++) {
            logmsg(hosts[i], "loaded from host file");
        }

        if (debug) {
            printf("Reloaded hosts list:\n");
            for (int i = 0; i < num_hosts; i++)
                printf("  %s\n", hosts[i]);
        }
    }
}
        for(int i=0;i<num_hosts;i++){
            int ok=check_host(hosts[i],"443");
            if(ok){
                if(state[i]==0) logmsg(hosts[i],"connectivity restored");
                state[i]=1;
                if(!global_connected && restored_host[0]=='\0')
                    strncpy(restored_host,hosts[i],sizeof(restored_host)-1);
                up=1;
            } else {
                if(state[i]!=0) logmsg(hosts[i],"unreachable");
                state[i]=0;
            }
        }

        if(up && !global_connected){ logmsg(restored_host,"Global connectivity restored"); global_connected=1;}
        if(!up && global_connected){ logmsg(NULL,"All hosts unreachable"); global_connected=0;}
        sleep(interval);
    }

    logmsg(NULL,"stopped");
    fclose(log_file);
    return 0;
}
