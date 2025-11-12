/*
 * chkinetup_win.c
 * Complete unified Windows build with help appended into GUI (v0.15).
 *
 * Build instructions:
 * - Linker->System->Subsystem = Windows (/SUBSYSTEM:WINDOWS)
 * - Run with -d to attach console, or double-click to show GUI by default.
 */

#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <lmcons.h>
#include <direct.h>
#include <tchar.h>
#include <stdarg.h>
#include <conio.h>
#include <signal.h>

 // -------------------- Constants --------------------
#define PROGRAM "chkinetup"
#define VERSION "v0.10"
#define MAX_HOSTS 50
#define HOSTNAME_LEN 128
#define USERNAME_LEN 64

// -------------------- Globals --------------------
HWND hMainWnd = NULL;
HWND hLogWindow = NULL;
static WNDPROC g_origEditProc = NULL;    // original edit proc (for subclass)
volatile sig_atomic_t stop_program = 0;
FILE* log_file = NULL;
int interval = 5;
int debug = 0;
char username[USERNAME_LEN] = "";
char hostname_buf[HOSTNAME_LEN] = "";
char* hosts[MAX_HOSTS];
int num_hosts = 0;
int state[MAX_HOSTS];
int global_connected = 1;
char logfile_fullpath[MAX_PATH] = "";
char logdir[MAX_PATH] = "";
bool hosts_are_builtin = true;
DWORD mainThreadId = 0;
bool console_attached = false;

// -------------------- Custom PostMessage codes --------------------
#define CMD_OPEN_GUI     (WM_APP+1)
#define CMD_QUIT         (WM_APP+2)
#define CMD_SHOW_HELP    (WM_APP+3)
#define CMD_SHOW_HOSTS   (WM_APP+4)
#define CMD_SHOW_LOGFILE (WM_APP+5)
#define CMD_INC_DELAY    (WM_APP+6)
#define CMD_DEC_DELAY    (WM_APP+7)

// -------------------- Forward declarations --------------------
void usage(FILE* stream);
void append_help_to_gui(void);
void attach_console_if_needed(void);
void logmsg(const char* host, const char* msgfmt, ...);
int check_host(const char* host, const char* port);
DWORD WINAPI CheckerThread(LPVOID lpParam);
DWORD WINAPI ConsoleInputThread(LPVOID lpParam);
HWND create_gui_window(void);
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void gui_show_hosts(void);
void gui_show_logfile_contents(void);
void append_to_gui_raw(const char* s);

// -------------------- Convert CommandLine to argv --------------------
static char** get_argv_from_CommandLineA(int* argc_out) {
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), argc_out);
    if (!wargv) return NULL;
    char** argv = (char**)malloc(sizeof(char*) * (*argc_out));
    for (int i = 0; i < *argc_out; ++i) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = (char*)malloc(len);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], len, NULL, NULL);
    }
    LocalFree(wargv);
    return argv;
}

// -------------------- Usage/help text --------------------
void usage(FILE* stream) {
    fprintf(stream,
        "%s %s - Internet connectivity checker\n\n"
        "Usage: %s [options] [delay]\n\n"
        "Options:\n"
        "  -h, --help              Show this help message and exit\n"
        "  -d, --debug             Enable debug output to screen\n"
        "  -l, --logfile <name>    Set logfile name (default: <hostname>.log)\n"
        "  -L, --logdir <path>     Set logfile directory (default: %%USERPROFILE%%\\\\log)\n"
        "  -c, --checkfile <file>  File containing list of hosts to check\n"
        "  -C, --clearfile         Ignore existing host file if exists\n"
        "  -H, --builtin-hosts     Use built-in host list\n"
        "  -v, --version           Show program version\n"
        "  -G, --gui               Force GUI window\n"
        "  -N, --nogui             Force console-only output\n"
        "  -q, --quit              Quit program immediately\n\n"
        "Positional args:\n"
        "  delay                   Interval in seconds between checks (default: 5)\n\n"
        "GUI mode key bindings:\n"
        "  ?: Show this help message (also 'h')\n"
        "  H: Display current host list (uppercase H)\n"
        "  D: Increase delay by 1 second\n"
        "  d: Decrease delay by 1 second\n"
        "  L: Show log file path and contents\n"
        "  Q: Quit program\n"
        "  G: Open GUI window if currently running in console only (or bring to front)\n\n",
        PROGRAM, VERSION, PROGRAM
    );
}

// Append the help text into the GUI log area (and to stdout if console is attached)
void append_help_to_gui(void) {
    // We'll create the same help text as usage() but append it into the GUI.
    // Keep it in one string so it's easy to append.
    const char* help_text =
        "=== Help ===\r\n"
        "Usage: " PROGRAM " [options] [delay]\r\n"
        "\r\n"
        "Options:\r\n"
        "  -h, --help              Show this help message and exit\r\n"
        "  -d, --debug             Enable debug output to screen\r\n"
        "  -l, --logfile <name>    Set logfile name (default: <hostname>.log)\r\n"
        "  -L, --logdir <path>     Set logfile directory (default: %USERPROFILE%\\log)\r\n"
        "  -c, --checkfile <file>  File containing list of hosts to check\r\n"
        "  -C, --clearfile         Ignore existing host file if exists\r\n"
        "  -H, --builtin-hosts     Use built-in host list\r\n"
        "  -v, --version           Show program version\r\n"
        "  -G, --gui               Force GUI window\r\n"
        "  -N, --nogui             Force console-only output\r\n"
        "  -q, --quit              Quit program immediately\r\n"
        "\r\n"
        "GUI mode key bindings:\r\n"
        "  ?: Show this help message (also 'h')\r\n"
        "  H: Display current host list (uppercase H)\r\n"
        "  D: Increase delay by 1 second\r\n"
        "  d: Decrease delay by 1 second\r\n"
        "  L: Show log file path and contents\r\n"
        "  Q: Quit program\r\n"
        "  G: Open GUI window if currently running in console only (or bring to front)\r\n"
        "\r\n";

    append_to_gui_raw(help_text);
    if (console_attached) {
        // Also print to stdout if console present
        usage(stdout);
    }
}

// -------------------- Attach console --------------------
void attach_console_if_needed(void) {
    if (console_attached) return;
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    console_attached = true;
}

// -------------------- Logging --------------------
void append_to_gui_raw(const char* s) {
    if (!hLogWindow) return;
    int len = GetWindowTextLengthA(hLogWindow);
    SendMessageA(hLogWindow, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(hLogWindow, EM_REPLACESEL, 0, (LPARAM)s);
    SendMessageA(hLogWindow, EM_SCROLLCARET, 0, 0);
}

void logmsg(const char* host, const char* msgfmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, msgfmt);
    vsnprintf(msg, sizeof(msg), msgfmt, ap);
    va_end(ap);

    time_t now = time(NULL);
    struct tm t;
    localtime_s(&t, &now);

    char line[768];
    snprintf(line, sizeof(line),
        "[%02d:%02d:%04d %02d:%02d:%02d] %s - %s",
        t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
        t.tm_hour, t.tm_min, t.tm_sec,
        host ? host : hostname_buf, msg
    );

    if (log_file) { fprintf(log_file, "%s\n", line); fflush(log_file); }
    if (console_attached) { printf("%s\n", line); fflush(stdout); }
    if (hLogWindow) { append_to_gui_raw(line); append_to_gui_raw("\r\n"); }
}

// -------------------- Check host --------------------
int check_host(const char* host, const char* port) {
    struct addrinfo hints = { 0 }, * res = NULL, * p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(host, port, &hints, &res);
    if (rv != 0) return 0;

    int result = 0;
    for (p = res; p != NULL; p = p->ai_next) {
        SOCKET s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;

        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);
        connect(s, p->ai_addr, (int)p->ai_addrlen);

        fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
        struct timeval tv = { 2,0 };
        int sel = select(0, NULL, &wfds, NULL, &tv);
        if (sel > 0) {
            int so_error = 0; int len = sizeof(so_error);
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
            if (so_error == 0) result = 1;
        }
        closesocket(s);
        if (result) break;
    }
    freeaddrinfo(res);
    return result;
}

// -------------------- GUI helpers --------------------
void gui_show_hosts(void) {
    if (!hLogWindow) return;
    append_to_gui_raw("\r\n");
    append_to_gui_raw("=== Current host list ===\r\n");
    append_to_gui_raw(hosts_are_builtin ? "(Built-in hosts)\r\n" : "(Custom hosts)\r\n");
    append_to_gui_raw("------------------------\r\n");
    for (int i = 0; i < num_hosts; i++) {
        append_to_gui_raw("  ");
        append_to_gui_raw(hosts[i]);
        append_to_gui_raw("\r\n");
    }
    append_to_gui_raw("\r\n");
}

void gui_show_logfile_contents(void) {
    if (!hLogWindow) return;
    if (logfile_fullpath[0] == '\0') { append_to_gui_raw("Log file not available.\r\n"); return; }
    append_to_gui_raw("=== Log file start ===\r\n");
    FILE* f = fopen(logfile_fullpath, "r");
    if (!f) { append_to_gui_raw("(unable to open log file)\r\n"); }
    else {
        char buf[512];
        while (fgets(buf, sizeof(buf), f)) append_to_gui_raw(buf);
        fclose(f);
    }
    append_to_gui_raw("=== Log file end ===\r\n");
    append_to_gui_raw("Logfile: ");
    append_to_gui_raw(logfile_fullpath);
    append_to_gui_raw("\r\n");
}

// -------------------- Edit subclass proc (forward keys to main window) --------------------
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN || msg == WM_CHAR) {
        if (hMainWnd) {
            // Forward as WM_CHAR so main window logic is unified
            PostMessageA(hMainWnd, WM_CHAR, wParam, lParam);
        }
    }
    return CallWindowProcA(g_origEditProc, hwnd, msg, wParam, lParam);
}

// -------------------- GUI Window Proc --------------------
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_CHAR: {
        char c = (char)wParam;
        if (c == '?' || c == 'h') {
            append_help_to_gui();
        }
        else if (c == 'H') {
            gui_show_hosts();
        }
        else if (c == 'D') {
            interval++;
            logmsg(NULL, "Delay increased to %d seconds", interval);
            append_to_gui_raw("=== Delay: ");
            char tmp[64]; snprintf(tmp, sizeof(tmp), "%d seconds ===\r\n", interval);
            append_to_gui_raw(tmp);
        }
        else if (c == 'd') {
            if (interval > 1) interval--;
            logmsg(NULL, "Delay decreased to %d seconds", interval);
            append_to_gui_raw("=== Delay: ");
            char tmp[64]; snprintf(tmp, sizeof(tmp), "%d seconds ===\r\n", interval);
            append_to_gui_raw(tmp);
        }
        else if (c == 'L') {
            gui_show_logfile_contents();
        }
        else if (c == 'Q' || c == 'q') {
            stop_program = 1;
            PostQuitMessage(0);
        }
        else if (c == 'G' || c == 'g') {
            if (hMainWnd) {
                if (IsIconic(hMainWnd)) ShowWindow(hMainWnd, SW_RESTORE);
                SetForegroundWindow(hMainWnd);
                FLASHWINFO fwi = { sizeof(fwi), hMainWnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 3, 0 };
                FlashWindowEx(&fwi);
            }
            else {
                create_gui_window();
            }
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        hMainWnd = hLogWindow = NULL;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

HWND create_gui_window(void) {
    if (hMainWnd) {
        if (IsIconic(hMainWnd)) ShowWindow(hMainWnd, SW_RESTORE);
        SetForegroundWindow(hMainWnd);
        return hMainWnd;
    }

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "CheckWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("CheckWindowClass", "Check Internet Sites Up",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 740, 520,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!hwnd) return NULL;
    hMainWnd = hwnd;

    hLogWindow = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        8, 8, 708, 480, hwnd, NULL, GetModuleHandle(NULL), NULL);

    // subclass the edit control so key strokes get forwarded
    g_origEditProc = (WNDPROC)SetWindowLongPtrA(hLogWindow, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // initial header: host list + delay + key hints
    append_to_gui_raw("=== Check Internet Sites Up ===\r\n");
    append_to_gui_raw("Keys: ?=Help (or 'h')  H=Hosts  D/d=Delay +/-  L=Log  Q=Quit  G=Open/Bring GUI\r\n");
    char header[256];
    snprintf(header, sizeof(header), "Current delay: %d seconds\r\n", interval);
    append_to_gui_raw(header);
    append_to_gui_raw("\r\n");

    // show hosts immediately
    gui_show_hosts();

    // ensure window receives keyboard focus
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    return hwnd;
}

// -------------------- Checker Thread --------------------
DWORD WINAPI CheckerThread(LPVOID lpParam) {
    (void)lpParam;
    while (!stop_program) {
        int up = 0; char restored_host[HOSTNAME_LEN] = { 0 };
        for (int i = 0; i < num_hosts; i++) {
            int ok = check_host(hosts[i], "443");
            if (ok) {
                if (state[i] == 0) logmsg(hosts[i], "connectivity restored");
                state[i] = 1;
                if (!global_connected && restored_host[0] == '\0') strncpy(restored_host, hosts[i], sizeof(restored_host) - 1);
                up = 1;
            }
            else {
                if (state[i] != 0) logmsg(hosts[i], "unreachable");
                state[i] = 0;
            }
        }
        if (up && !global_connected) { logmsg(restored_host, "Global connectivity restored"); global_connected = 1; }
        if (!up && global_connected) { logmsg(NULL, "All hosts unreachable"); global_connected = 0; }

        for (int slept = 0; slept < interval && !stop_program; slept++) Sleep(1000);
    }
    return 0;
}

// -------------------- Console input thread --------------------
DWORD WINAPI ConsoleInputThread(LPVOID lpParam) {
    (void)lpParam;
    while (!stop_program) {
        if (_kbhit()) {
            int raw = _getch();
            if (raw == 0 || raw == 224) { (void)_getch(); continue; } // extended keys
            char ch = (char)raw;
            if (ch == 'G' || ch == 'g') PostThreadMessage(mainThreadId, CMD_OPEN_GUI, 0, 0);
            else if (ch == 'Q' || ch == 'q') PostThreadMessage(mainThreadId, CMD_QUIT, 0, 0);
            else if (ch == 'H') PostThreadMessage(mainThreadId, CMD_SHOW_HOSTS, 0, 0);
            else if (ch == 'h' || ch == '?') PostThreadMessage(mainThreadId, CMD_SHOW_HELP, 0, 0);
            else if (ch == 'L' || ch == 'l') PostThreadMessage(mainThreadId, CMD_SHOW_LOGFILE, 0, 0);
            else if (ch == 'D') PostThreadMessage(mainThreadId, CMD_INC_DELAY, 0, 0);
            else if (ch == 'd') PostThreadMessage(mainThreadId, CMD_DEC_DELAY, 0, 0);
        }
        Sleep(100);
    }
    return 0;
}

// -------------------- Ctrl+C handler --------------------
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT) { stop_program = 1; return TRUE; }
    return FALSE;
}

// -------------------- Main logic --------------------
int run_with_argv(int argc, char** argv) {
    bool forceGUI = false, forceConsole = false;
    char logfile_name[MAX_PATH] = "", checkfile[MAX_PATH] = "";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(stdout); return 0; }
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) debug = 1;
        if (strcmp(argv[i], "-G") == 0 || strcmp(argv[i], "--gui") == 0) forceGUI = true;
        if (strcmp(argv[i], "-N") == 0 || strcmp(argv[i], "--nogui") == 0) forceConsole = true;
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) { printf("%s %s\n", PROGRAM, VERSION); return 0; }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quit") == 0) return 0;
        if ((strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--logfile") == 0) && i + 1 < argc) strncpy(logfile_name, argv[++i], sizeof(logfile_name) - 1);
        if ((strcmp(argv[i], "-L") == 0 || strcmp(argv[i], "--logdir") == 0) && i + 1 < argc) strncpy(logdir, argv[++i], sizeof(logdir) - 1);
        if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--checkfile") == 0) && i + 1 < argc) strncpy(checkfile, argv[++i], sizeof(checkfile) - 1);
        int val = atoi(argv[i]); if (val > 0) interval = val;
    }

    if (logdir[0] == '\0') { const char* up = getenv("USERPROFILE"); if (!up) up = "."; snprintf(logdir, sizeof(logdir), "%s\\log", up); }
    _mkdir(logdir);

    DWORD ulen = UNLEN + 1; GetUserNameA(username, &ulen);
    gethostname(hostname_buf, sizeof(hostname_buf));

    if (logfile_name[0]) snprintf(logfile_fullpath, sizeof(logfile_fullpath), "%s\\%s", logdir, logfile_name);
    else snprintf(logfile_fullpath, sizeof(logfile_fullpath), "%s\\%s.log", logdir, hostname_buf);

    log_file = fopen(logfile_fullpath, "a");

    if (checkfile[0]) {
        FILE* cf = fopen(checkfile, "r");
        if (cf) {
            char line[256]; num_hosts = 0; hosts_are_builtin = false;
            while (fgets(line, sizeof(line), cf) && num_hosts < MAX_HOSTS) {
                line[strcspn(line, "\r\n")] = 0; if (line[0] == '\0') continue;
                hosts[num_hosts++] = _strdup(line);
            }
            fclose(cf);
        }
        else hosts_are_builtin = true;
    }
    if (num_hosts == 0) {
        const char* default_hosts[] = { "www.google.com","www.cloudflare.com","www.microsoft.com","www.amazon.com" };
        num_hosts = 4; hosts_are_builtin = true;
        for (int i = 0; i < num_hosts; i++) hosts[i] = _strdup(default_hosts[i]);
    }
    for (int i = 0; i < num_hosts; i++) state[i] = -1;

    console_attached = (forceConsole || debug);
    if (console_attached) attach_console_if_needed();
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { if (console_attached) fprintf(stderr, "WSAStartup failed\n"); return 1; }

    mainThreadId = GetCurrentThreadId();

    // Create GUI by default when double-clicked (no console attached) unless forced otherwise
    if (forceGUI || (!console_attached && !forceConsole)) create_gui_window();

    // start threads
    HANDLE hChecker = CreateThread(NULL, 0, CheckerThread, NULL, 0, NULL);
    HANDLE hConsole = NULL;
    if (console_attached) hConsole = CreateThread(NULL, 0, ConsoleInputThread, NULL, 0, NULL);

    // main loop: process posted messages and window messages, but don't block on GetMessage
    MSG msg;
    while (!stop_program) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == CMD_QUIT) { stop_program = 1; }
            else if (msg.message == CMD_OPEN_GUI) create_gui_window();
            else if (msg.message == CMD_SHOW_HOSTS) gui_show_hosts();
            else if (msg.message == CMD_SHOW_LOGFILE) gui_show_logfile_contents();
            else if (msg.message == CMD_INC_DELAY) {
                interval++;
                logmsg(NULL, "Delay increased to %d seconds", interval);
                append_to_gui_raw("=== Delay: ");
                char tmp[64]; snprintf(tmp, sizeof(tmp), "%d seconds ===\r\n", interval); append_to_gui_raw(tmp);
            }
            else if (msg.message == CMD_DEC_DELAY) {
                if (interval > 1) interval--;
                logmsg(NULL, "Delay decreased to %d seconds", interval);
                append_to_gui_raw("=== Delay: ");
                char tmp2[64]; snprintf(tmp2, sizeof(tmp2), "%d seconds ===\r\n", interval); append_to_gui_raw(tmp2);
            }
            else if (msg.message == CMD_SHOW_HELP) {
                append_help_to_gui();
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(50);
    }

    // cleanup
    if (hChecker) { WaitForSingleObject(hChecker, INFINITE); CloseHandle(hChecker); }
    if (hConsole) { WaitForSingleObject(hConsole, INFINITE); CloseHandle(hConsole); }

    WSACleanup();
    if (log_file) fclose(log_file);
    return 0;
}

// -------------------- WinMain --------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc; char** argv = get_argv_from_CommandLineA(&argc);
    int res = run_with_argv(argc, argv);
    if (argv) { for (int i = 0; i < argc; ++i) free(argv[i]); free(argv); }
    return res;
}