/* USER RE-DEFINABLE MACROS */

#ifndef CB_BUF_SIZE // Avoids redefinition error if '-DCB_BUF_SIZE=<number>' is used
    /* Sets the size of text buffers */
    #define CB_BUF_SIZE 262144 // Change the value to change the size of text buffers
#endif

#ifndef CB_PROG_LOGIC_MAX // Avoids redefinition error if '-DCB_PROG_LOGIC_MAX=<number>' is used
    /* Sets the size of logic command buffers */
    #define CB_PROG_LOGIC_MAX 256 // Change the value to change how far logic commands can be nested
#endif

/* Uses strcpy and strcat in place of copyStr and copyStrApnd */
#define BUILT_IN_STRING_FUNCS // Comment out this line to use CLIBASIC string functions

/* Uses strcmp in place of custom code */
#define BUILT_IN_COMMAND_COMPARE // Comment out this line to use the custom compare code when comparing commands and function strings

/* Sets what file CLIBASIC uses to store command history */
#define HIST_FILE ".clibasic_history" // Change the value to change where CLIBASIC puts the history file

/* Changes the terminal title to display the CLIBASIC version and bits */
#define CHANGE_TITLE // Comment out this line to disable changing the terminal/console title

/* Opens in XTerm if CLIBASIC is not started in a terminal */
#define GUI_CHECK // Comment out this line if you do not have a tty command or your tty command is missing the -s option

#ifdef _WIN32 // Avoids defining _WIN_NO_VT on a non-Windows operating system
    /* Disables ANSI/VT escape codes */
    #define _WIN_NO_VT // Comment out this line if you are using Windows 10 build 16257 or later
#endif

/* ------------------------ */

// Patch/checking defines/compares

/* Fix implicit declaration issues */
#define _POSIX_C_SOURCE 999999L
#define _XOPEN_SOURCE 999999L

/* Check if the buffer size is usable */
#if (CB_BUF_SIZE < 0)
    #error /* CB_BUF_SIZE cannot be less than 0 */ \
    Invalid value for CB_BUF_SIZE (CB_BUF_SIZE cannot be less than 0).
#elif (CB_BUF_SIZE < 2)
    #error /* Buffers only have room for NULL char */ \
    Unusable CB_BUF_SIZE (Buffers only have room for NULL char).
#elif (CB_BUF_SIZE < 256)
    #warning /* Buffers will be small and may cause issues */ \
    Small CB_BUF_SIZE (Buffers will be small and may cause issues).
#endif

/* Check if the logic command buffer sizes are usable */
#if (CB_PROG_LOGIC_MAX < 1)
    #error /* CB_PROG_LOGIC_MAX cannot be less than 1 */ \
    Invalid value for CB_PROG_LOGIC_MAX (CB_PROG_LOGIC_MAX cannot be less than 1).
#elif (CB_PROG_LOGIC_MAX < 2)
    #warning /* Logic commands cannot be nested */ \
    Microscopic CB_PROG_LOGIC_MAX (Logic commands cannot be nested).
#elif (CB_PROG_LOGIC_MAX < 16)
    #warning /* Logic commands will max out easily */ \
    Small CB_PROG_LOGIC_MAX (Logic commands will max out easily).
#endif

#ifndef _WIN32 // NOTE: Assumes Unix-based system on anything except Windows
    #ifndef __unix__
        #define __unix__
    #endif
#endif

// Needed includes

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>

// OS-specific includes and definitions

#ifndef _WIN32
    #include <termios.h>
    #include <sys/ioctl.h>
    #include <sys/wait.h>
#else
    #include <windows.h>
    #include <conio.h>
    #include <fileapi.h>
#endif

#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

// More patch/checking defines/compares

/* Swap function to swap values */
#define swap(a, b) __typeof__(a) c = a; a = b; b = c

// Base defines

char VER[] = "0.19";

#if defined(__linux__)
    char OSVER[] = "Linux";
#elif defined(BSD)
    char OSVER[] = "BSD";
#elif defined(__APPLE__)
    char OSVER[] = "MacOS";
#elif defined(__unix__)
    char OSVER[] = "Unix";
#elif defined(_WIN32)
    char OSVER[] = "Windows";
#else
    #warning /* No matching operating system defines */ \
    Could not detect operating system. (No matching operating system defines)
    char OSVER[] = "?";
#endif

#ifdef B32
    char BVER[] = "32";
#elif B64
    char BVER[] = "64";
#else
    #warning /* Neither B32 or B64 was defined */ \
    Could not detect architecture bits, please use '-DB64' or '-DB32' when compiling (Neither B32 or B64 was defined).
    char BVER[] = "?";
#endif

// Global vars and functions

int progindex = -1;
char** progbuf = NULL;
char** progfn = NULL;
char* progfnstr = NULL;
int64_t* progcp = NULL;
int* progcmdl = NULL;
int* proglinebuf = NULL;

int err = 0;
int cerr;

bool inProg = false;
bool chkinProg = false;
int progLine = 1;

int varmaxct = 0;

typedef struct {
    int32_t size;
    uint8_t type;
    char** data;
} cb_var;

char** varname = NULL;
cb_var* vardata = NULL;
bool* varinuse = NULL;

char gpbuf[CB_BUF_SIZE];

char* cmd;
int cmdl;
char** tmpargs;
char** arg;
uint8_t* argt;
int32_t* argl;
int argct = 0;

int cmdpos;

bool didloop = false;
bool lockpl = false;

int dlstack[CB_PROG_LOGIC_MAX];
int dlpline[CB_PROG_LOGIC_MAX];
bool dldcmd[CB_PROG_LOGIC_MAX];
int dlstackp = -1;

bool itdcmd[CB_PROG_LOGIC_MAX];
int itstackp = -1;
bool didelse = false;

int fnstack[CB_PROG_LOGIC_MAX];
bool fndcmd[CB_PROG_LOGIC_MAX];
bool fninfor[CB_PROG_LOGIC_MAX];
int fnpline[CB_PROG_LOGIC_MAX];
int fnstackp = -1;
char fnvar[CB_BUF_SIZE];
char forbuf[4][CB_BUF_SIZE];

char* errstr;

char conbuf[CB_BUF_SIZE];
char prompt[CB_BUF_SIZE];
char pstr[CB_BUF_SIZE];

uint8_t fgc = 15;
uint8_t bgc = 0;

int curx = 0;
int cury = 0;

int concp = 0;
int64_t cp = 0;

bool cmdint = false;
bool inprompt = false;

bool runfile = false;
bool runc = false;

bool sh_silent = false;
bool sh_clearAttrib = true;
bool sh_restoreAttrib = true;

bool txt_bold = false;
bool txt_italic = false;
bool txt_underln = false;
bool txt_underlndbl = false;
bool txt_underlnsqg = false;
bool txt_strike = false;
bool txt_overln = false;
bool txt_dim = false;
bool txt_blink = false;
bool txt_hidden = false;
bool txt_reverse = false;
bool txt_fgc = true;
bool txt_bgc = false;
int  txt_underlncolor = 0;

bool textlock = false;
bool sneaktextlock = false;

bool keep = false;

char* homepath = NULL;
char* cwd = NULL;

bool autohist = false;

int tab_width = 4;

int progargc = 0;
char** progargs = NULL;
char* startcmd = NULL;

bool changedtitle = false;
bool changedtitlecmd = false;

#ifdef __unix__
struct termios term, restore;

void txtqunlock() {if (textlock || sneaktextlock) {tcsetattr(0, TCSANOW, &restore); textlock = false;}}

int kbhit() {
    static const int STDIN = 0;
    static bool initialized = false;
    if (!initialized) {
        struct termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }
    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}
#endif

static inline void* setsig(int sig, void* func) {
    #ifdef __unix__
    struct sigaction act, old;
    memset (&act, 0, sizeof(act));
    memset (&old, 0, sizeof(old));
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, sig);
    act.sa_handler = func;
    act.sa_flags = SA_RESTART;
    sigaction(sig, &act, &old);
    return old.sa_handler;
    #else
    return signal(sig, func);
    #endif
}

int tab_end = 0;
static inline void strApndChar(char*, char);
char* rl_get_tab(const char* text, int state) {
    char* tab = NULL;
    rl_filename_quoting_desired = 0;
    if (!state) {
        tab = malloc(strlen(text) + 5);
        strcpy(tab, text);
        tab_end = tab_width - (tab_end % tab_width) - 1;
        if (!tab_end) tab_end = tab_width;
        fflush(stdout);
        for (int i = 0; i < tab_end; ++i) {
            strApndChar(tab, ' ');
        }
    }
    return tab;
}
char** rl_tab(const char* text, int start, int end) {
    (void)start;
    char** tab = NULL;
    tab_end = end;
    tab = rl_completion_matches(text, rl_get_tab);
    return tab;
}

#ifdef _WIN32
#define hConsole GetStdHandle(STD_OUTPUT_HANDLE)
char* rlptr;
bool inrl = false;
void cleanExit();
void setcsr() {
    COORD coord;
    coord.X = 0;
    coord.Y = 0;
    SetConsoleCursorPosition(hConsole, coord);
}
void rl_sigh(int sig) {
    setsig(sig, rl_sigh);
    putchar('\n');
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}
void txtqunlock() {}
//HANDLE hConsole;
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 4
#endif
char kbinbuf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
bool winArgNeedsQuotes(char* str) {
    for (int32_t i = 0; str[i]; ++i) {
        switch (str[i]) {
            case ' ':
            case '&':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '^':
            case '=':
            case ';':
            case '!':
            case '\'':
            case '+':
            case ',':
            case '`':
            case '~':
                return true;
                break;
        }
    }
    return false;
}
#pragma GCC diagnostic pop
#ifdef _WIN_NO_VT
WORD ocAttrib = 0;
#endif
#endif

struct timeval time1;
uint64_t tval;

void* oldsigh = NULL;

void nokill() {
    if (!oldsigh) oldsigh = setsig(SIGINT, nokill);
}

void yeskill() {
    setsig(SIGINT, oldsigh);
    oldsigh = NULL;
}

void forceExit() {
    #ifndef _WIN32
    txtqunlock();
    #endif
    exit(0);
}

static inline void getCurPos();

char* gethome();

inline void initBaseMem();
inline void freeBaseMem();

void printError(int);

static inline void seterrstr(char*);

void unloadAllProg();

char* basefilename(char*);

void cleanExit() {
    txtqunlock();
    if (inprompt) {
        int i = kbhit();
        while (i > 0) {getchar(); i--;}
        unloadAllProg();
        cmdint = true;
        inProg = false;
        putchar('\n');
        rl_on_new_line();
        rl_replace_line("", 0);
        rl_redisplay();
        return;
    }
    setsig(SIGINT, forceExit);
    setsig(SIGTERM, forceExit);
    int ret = chdir(gethome());
    (void)ret;
    if (autohist && !runfile) {
        write_history(HIST_FILE);
        #ifdef _WIN32
        SetFileAttributesA(HIST_FILE, FILE_ATTRIBUTE_HIDDEN);
        #endif
    }
    #if defined(CHANGE_TITLE) && !defined(_WIN_NO_VT)
    if (changedtitle) fputs("\e[23;0t", stdout);
    #endif
    #ifndef _WIN_NO_VT
    if (!keep) fputs("\e[0m", stdout);
    #else
    if (!keep) SetConsoleTextAttribute(hConsole, ocAttrib);
    #endif
    do {getCurPos();} while (!curx);
    if (curx != 1) putchar('\n');
    #ifdef __unix__
    int i = kbhit();
    while (i > 0) {getchar(); i--;}
    #endif
    #ifndef _WIN_NO_VT
    fputs("\e[2K", stdout);
    #endif
    freeBaseMem();
    exit(err);
}

void cmdIntHndl() {
    setsig(SIGINT, cmdIntHndl);
    if (cmdint) setsig(SIGINT, cleanExit);
    int i = kbhit();
    while (i > 0) {getchar(); i--;}
    cmdint = true;
}

void runcmd();
#ifdef BUILT_IN_STRING_FUNCS
    #define copyStr(b, a) strcpy(a, b)
    #define copyStrApnd(b, a) strcat(a, b)
#else
    static inline void copyStr(char* str1, char* str2);
    static inline void copyStrApnd(char* str1, char* str2);
#endif
static inline void copyStrSnip(char* str1, int32_t i, int32_t j, char* str2);
uint8_t getVal(char* inbuf, char* outbuf);
static inline void resetTimer();
bool loadProg(char* filename);
void unloadProg();
void updateTxtAttrib();
static inline bool isFile();
static inline uint64_t usTime();

char* gethome() {
    if (!homepath) {
        #ifdef __unix__
        homepath = getenv("HOME");
        #else
        char* str1 = getenv("HOMEDRIVE");
        char* str2 = getenv("HOMEPATH");
        homepath = (char*)malloc(strlen(str1) + strlen(str2) + 1);
        copyStr(str1, homepath);
        copyStrApnd(str2, homepath);
        #endif
    }
    return homepath;
}

char* bfnbuf = NULL;

char* basefilename(char* fn) {
    int32_t fnlen = strlen(fn);
    int32_t i;
    for (i = fnlen; i > -1; --i) {
        if (fn[i] == '/') break;
        #ifdef _WIN32
        if (fn[i] == '\\') break;
        #endif
    }
    copyStrSnip(fn, i + 1, fnlen, bfnbuf);
    return bfnbuf;
}

char* pathfilename(char* fn) {
    int32_t fnlen = strlen(fn);
    int32_t i;
    for (i = fnlen; i > -1; --i) {
        if (fn[i] == '/') break;
        #ifdef _WIN32
        if (fn[i] == '\\') break;
        #endif
    }
    copyStrSnip(fn, 0, i + 1, bfnbuf);
    return bfnbuf;
}

void ttycheck() {
    if (!isatty(STDERR_FILENO)) {exit(1);}
    if (!isatty(STDIN_FILENO)) {fputs("CLIBASIC does not support STDIN redirection.\n", stderr); exit(1);}
    if (!isatty(STDOUT_FILENO)) {fputs("CLIBASIC does not support STDOUT redirection.\n", stderr); exit(1);}
}

int main(int argc, char** argv) {
    #if defined(GUI_CHECK) && defined(__unix__)
    if (system("tty -s 1> /dev/null 2> /dev/null")) {
        char* command = malloc(CB_BUF_SIZE);
        copyStr("xterm -T CLIBASIC -b 0 -bg black -bcn 200 -bcf 200 -e 'echo -e -n \"\x1b[\x33 q\" && ", command);
        copyStrApnd(argv[0], command);
        strApndChar(command, '\'');
        int ret = system(command);
        free(command);
        exit(ret);
    }
    #endif
    bool pexit = false;
    bool skip = false;
    bool info = false;
    for (int i = 1; i < argc; ++i) {
        int shortopti = 0;
        bool shortopt;
        if (argv[i][0] == '-' && strcmp(argv[i], "--file") && strcmp(argv[i], "-f")) {
            chkshortopt:;
            shortopt = false;
            if (!argv[i][1]) {
                fputs("No short option following dash.\n", stderr); exit(1);
            } else if (argv[i][1] != '-') {
                shortopt = true; ++shortopti;
            }
            if (!argv[i][shortopti]) continue;
            if (!strcmp(argv[i], "--")) {
                fputs("No long option following dash.\n", stderr); exit(1);
            } else if (!strcmp(argv[i], "--version")) {
                if (argc > 2) {fputs("Incorrect number of options passed.\n", stderr); exit(1);}
                printf("Command Line Interface BASIC version %s (%s %s-bit)\n", VER, OSVER, BVER);
                puts("Copyright (C) 2021 PQCraft");
                puts("This software is licensed under the GNU GPL v3.");
                pexit = true;
            } else if (!strcmp(argv[i], "--help")) {
                if (argc > 2) {fputs("Incorrect number of options passed.\n", stderr); exit(1);}
                printf("Usage: %s [options] {[[{-f|--file}] FILE] [options] [--args [ARG...]] | --exec FILE [ARG...]}\n", argv[0]);
                puts("Options:");
                puts("    --help                      Shows this help text.");
                puts("    --version                   Shows the version and license information.");
                puts("    --args [ARG...]             Passes ARGs to the program.");
                puts("    {-x|--exec} FILE [ARG...]   Runs and passes ARGs to FILE.");
                puts("    {-f|--file} FILE            Runs FILE.");
                puts("    {-c|--command} COMMAND      Runs COMMAND and exits.");
                puts("    {-k|--keep}                 Stops CLIBASIC from resetting text attributes.");
                puts("    {-s|--skip}                 Skips searching for autorun programs.");
                puts("    {-i|--info}                 Enables the info text.");
                pexit = true;
            } else if (!strcmp(argv[i], "--args")) {
                if (runc || !runfile) {fputs("Args can only be passed when running a program.\n", stderr); exit(1);}
                progargs = (char**)malloc((argc - i) * sizeof(char*));
                for (progargc = 1; progargc < argc - i; ++progargc) {
                    progargs[progargc] = argv[i + progargc];
                }
                progargs[0] = progfn[progindex];
                i = argc;
            } else if (!strcmp(argv[i], "--exec") || !strcmp(argv[i], "-x")) {
                if (runc) {fputs("Cannot run command and file.\n", stderr); exit(1);}
                if (runfile) {fputs("Program already loaded, use --args.\n", stderr); exit(1); unloadAllProg();}
                ttycheck();
                if (runfile) {unloadProg(); fputs("Incorrect number of options passed.\n", stderr); exit(1);}
                i++;
                if (!argv[i]) {fputs("No filename provided.\n", stderr); exit(1);}
                if (!loadProg(argv[i])) {printError(cerr); exit(1);}
                inProg = true;
                runfile = true;
                progargs = (char**)malloc((argc - i) * sizeof(char*));
                for (progargc = 1; progargc < argc - i; ++progargc) {
                    progargs[progargc] = argv[i + progargc];
                }
                progargs[0] = progfn[progindex];
                i = argc;
            } else if (!strcmp(argv[i], "--keep") || (shortopt && argv[i][shortopti] == 'k')) {
                if (keep) {fputs("Incorrect number of options passed.\n", stderr); exit(1);}
                keep = true;
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--skip") || (shortopt && argv[i][shortopti] == 's')) {
                if (skip) {fputs("Incorrect number of options passed.\n", stderr); exit(1);}
                skip = true;
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--info") || (shortopt && argv[i][shortopti] == 'i')) {
                if (info) {fputs("Incorrect number of options passed.\n", stderr); exit(1);}
                info = true;
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--command") || !strcmp(argv[i], "-c")) {
                if (runfile) {fputs("Cannot run file and command.\n", stderr); exit(1);}
                ttycheck();
                if (runc) {fputs("Incorrect number of options passed.\n", stderr); exit(1);}
                i++;
                if (!argv[i]) {fputs( "No command provided.\n", stderr); exit(1);}
                runc = true;
                runfile = true;
                copyStr(argv[i], conbuf);
            } else if (shortopt && (argv[i][shortopti] == 'c' || argv[i][shortopti] == 'f' || argv[i][shortopti] == 'x')) {
                fprintf(stderr, "Short option '%c' requires argument and cannot be grouped.\n", argv[i][shortopti]); exit(1);
            } else {
                if (shortopt) {
                    fprintf(stderr, "Invalid short option '%c'.\n", argv[i][shortopti]); exit(1);
                } else {
                    fprintf(stderr, "Invalid option '%s'.\n", argv[i]); exit(1);
                }
            }
        } else {
            if (runc) {fputs("Cannot run command and file.\n", stderr); exit(1);}
            ttycheck();
            if (runfile) {unloadProg(); fputs("Incorrect number of options passed.\n", stderr); exit(1);}
            if (!strcmp(argv[i], "--file") || !strcmp(argv[i], "-f")) {
                i++;
                if (!argv[i]) {fputs("No filename provided.\n", stderr); exit(1);}
            }
            if (!loadProg(argv[i])) {printError(cerr); exit(1);}
            inProg = true;
            runfile = true;
        }
    }
    if (pexit) exit(0);
    ttycheck();
    rl_readline_name = "CLIBASIC";
    char* rl_tmpptr = calloc(1, 1);
    rl_completion_entry_function = rl_get_tab;
    rl_attempted_completion_function = (rl_completion_func_t*)rl_tab;
    rl_special_prefixes = rl_tmpptr;
    rl_completer_quote_characters = rl_tmpptr;
    rl_completer_word_break_characters = rl_tmpptr;
    #ifdef _WIN32
    if (hConsole == INVALID_HANDLE_VALUE) {
        fputs("Failed to open a valid console handle.\n", stderr);
        exit(GetLastError());
    }
    #ifndef _WIN_NO_VT
    DWORD dwMode = 0;
    if (!GetConsoleMode(hConsole, &dwMode)){
        fputs("Failed to get the console mode.\n", stderr);
        exit(GetLastError());
    }
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hConsole, dwMode)) {
        fputs("Failed to set the console mode.\n", stderr);
        exit(GetLastError());
    }
    #else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    ocAttrib = csbi.wAttributes;
    #endif
    #endif
    setsig(SIGINT, cleanExit);
    setsig(SIGTERM, cleanExit);
    #ifndef _WIN32
    setsig(SIGQUIT, forceExit);
    #else
    setsig(SIGABRT, forceExit);
    #endif
    startcmd = malloc(CB_BUF_SIZE);
    #ifndef _WIN32
        #ifndef __APPLE__
            #ifndef __FreeBSD__
                int32_t scsize;
                if ((scsize = readlink("/proc/self/exe", startcmd, CB_BUF_SIZE)) == -1)
                    #ifndef __linux__
                    if ((scsize = readlink("/proc/curproc/file", startcmd, CB_BUF_SIZE)) == -1)
                    #endif
                        goto scargv;
                startcmd = realloc(startcmd, scsize + 1);
            #else
                int mib[4];
                mib[0] = CTL_KERN;
                mib[1] = KERN_PROC;
                mib[2] = KERN_PROC_PATHNAME;
                mib[3] = -1;
                size_t cb = CB_BUF_SIZE;
                sysctl(mib, 4, startcmd, &cb, NULL, 0);
                startcmd = realloc(startcmd, strlen(startcmd) + 1);
                char* tmpstartcmd = realpath(startcmd, NULL);
                swap(startcmd, tmpstartcmd);
                free(tmpstartcmd);
                startcmd = realloc(startcmd, strlen(startcmd) + 1);
            #endif
        #else
            uint32_t tmpsize = CB_BUF_SIZE;
            if (_NSGetExecutablePath(startcmd, &size)) {
                goto scargv;
            }
            char* tmpstartcmd = realpath(startcmd, NULL);
            swap(startcmd, tmpstartcmd);
            free(tmpstartcmd);
            startcmd = realloc(startcmd, strlen(startcmd) + 1);
        #endif
    #else
        if (GetModuleFileName(NULL, startcmd, CB_BUF_SIZE)) {
            startcmd = realloc(startcmd, strlen(startcmd) + 1);
        } else {
            goto scargv;
        }
    #endif
    goto skipscargv;
    scargv:;
    if (strcmp(argv[0], basefilename(argv[0]))) {
        free(startcmd);
        #ifndef _WIN32
        startcmd = realpath(argv[0], NULL);
        #else
        startcmd = _fullpath(NULL, argv[0], CB_BUF_SIZE);
        #endif
    } else {
        startcmd = argv[0];
    }
    skipscargv:;
    getCurPos();
    if (curx != 1) putchar('\n');
    updateTxtAttrib();
    if (!runfile) {
        if (info) printf("Command Line Interface BASIC version %s (%s %s-bit)\n", VER, OSVER, BVER);
        strcpy(prompt, "\"CLIBASIC> \"");
        #ifdef CHANGE_TITLE
        #ifdef __unix__
        fputs("\e[22;0t", stdout);
        changedtitle = true;
        printf("\e]2;CLIBASIC %s (%s-bit)%c", VER, BVER, 7);
        fflush(stdout);
        #else
        char* tmpstr = (char*)malloc(CB_BUF_SIZE);
        sprintf(tmpstr, "CLIBASIC %s (%s-bit)", VER, BVER);
        SetConsoleTitleA(tmpstr);
        free(tmpstr);
        #endif
        #endif
    }
    errstr = NULL;
    cmd = NULL;
    argt = NULL;
    arg = NULL;
    srand(usTime());
    if (!runfile) {
        if (!gethome()) {
            #ifdef __unix__
            fputs("Could not find home folder! Please set the 'HOME' environment variable.", stderr);
            #else
            fputs("Could not find home folder!", stderr);
            #endif
        } else if (!skip) {
            char* tmpcwd = getcwd(NULL, 0);
            int ret = chdir(homepath);
            FILE* tmpfile = fopen(HIST_FILE, "r");
            if ((autohist = (bool)tmpfile)) {fclose(tmpfile); read_history(HIST_FILE);}
            inProg = true;
            if (!loadProg("AUTORUN.BAS")) if (!loadProg("autorun.bas")) inProg = false;
            ret = chdir(tmpcwd);
            (void)ret;
        }
    }
    cerr = 0;
    initBaseMem();
    resetTimer();
    while (!pexit) {
        fchkint:;
        cp = 0;
        if (chkinProg) {inProg = true; chkinProg = false;}
        if (!inProg && !runc) {
            if (runfile) cleanExit();
            char* tmpstr = NULL;
            int tmpt = getVal(prompt, pstr);
            if (tmpt != 1) strcpy(pstr, "CLIBASIC> ");
            getCurPos();
            #ifdef __unix__
            curx--;
            int32_t ptr = strlen(pstr);
            while (curx > 0) {pstr[ptr] = 22; ptr++; curx--;}
            pstr[ptr] = 0;
            #else
            if (curx > 1) putchar('\n');
            #endif
            updateTxtAttrib();
            inprompt = true;
            #ifdef _WIN32
            setsig(SIGINT, rl_sigh);
            #endif
            conbuf[0] = 0;
            #ifdef __unix__
            setsig(SIGINT, cleanExit);
            #endif
            txtqunlock();
            tmpstr = readline(pstr);
            concp = 0;
            inprompt = false;
            if (!tmpstr) cleanExit();
            int32_t tmpptr;
            if (tmpstr[0] == 0) {free(tmpstr); goto brkproccmd;}
            for (tmpptr = 0; tmpstr[tmpptr] == ' '; ++tmpptr) {}
            if (tmpptr) {
                int32_t iptr;
                for (iptr = 0; tmpstr[tmpptr]; ++tmpptr, ++iptr) {
                    tmpstr[iptr] = tmpstr[tmpptr];
                }
                tmpstr[iptr] = 0;
            }
            if (tmpstr[0] == 0) {free(tmpstr); goto brkproccmd;}
            tmpptr = strlen(tmpstr);
            if (tmpptr--) {
                while (tmpstr[tmpptr] == ' ' && tmpptr) {
                    tmpstr[tmpptr--] = 0;
                }
            }
            HIST_ENTRY* tmphist = history_get(history_length);
            if (!tmphist || strcmp(tmpstr, tmphist->line)) add_history(tmpstr);
            copyStr(tmpstr, conbuf);
            free(tmpstr);
            cmdint = false;
        }
        if (runc) runc = false;
        cmdl = 0;
        if (chkinProg) {
            dlstackp = -1;
            itstackp = -1;
            fnstackp = -1;
            for (int i = 0; i < CB_PROG_LOGIC_MAX; ++i) {
                dlstack[i] = 0;
                dldcmd[i] = false;
                fnstack[i] = -1;
                fndcmd[i] = false;
                fninfor[i] = false;
                itdcmd[i] = false;
            }
        }
        didloop = false;
        didelse = false;
        bool inStr = false;
        if (!runfile) setsig(SIGINT, cmdIntHndl);
        progLine = 1;
        bool comment = false;
        while (1) {
            #ifdef _WIN32
            if (!textlock) {
                char kbc;
                int kbh = kbhit();
                for (int i = 0; i < kbh; ++i) {
                    kbc = _getch();
                    kbinbuf[i] = kbc;
                    putchar(kbc);
                    if (kbc == 3) {
                        if (inProg) {goto brkproccmd;}
                        else {cmdIntHndl();}
                    }
                }
                fflush(stdout);
            }
            #endif
            rechk:;
            if (progindex < 0) {inProg = false;}
            else if (inProg == false) {progindex = - 1;}
            if (inProg) {
                if (progbuf[progindex][cp] == '"') {inStr = !inStr; cmdl++;} else
                if ((progbuf[progindex][cp] == ':' && !inStr) || progbuf[progindex][cp] == '\n' || progbuf[progindex][cp] == 0) {
                    if (progbuf[progindex][cp - cmdl - 1] == '\n' && !lockpl) progLine++;
                    if (lockpl) lockpl = false;
                    while (progbuf[progindex][cp - cmdl] == ' ' && cmdl > 0) {cmdl--;}
                    cmd = (char*)realloc(cmd, cmdl + 1);
                    cmdpos = cp - cmdl;
                    copyStrSnip(progbuf[progindex], cp - cmdl, cp, cmd);
                    cmdl = 0;
                    runcmd();
                    if (cmdint) {inProg = false; unloadAllProg(); cmdint = false; goto brkproccmd;}
                    if (cp == -1) {inProg = false; unloadAllProg(); goto brkproccmd;}
                    if (cp > -1 && progbuf[progindex][cp] == 0) {
                        unloadProg();
                        if (progindex < 0) {
                            inProg = false;
                            goto rechk;
                        } else {
                            didloop = true;
                        }
                    }
                } else
                {cmdl++;}
                if (!didloop) {cp++;} else {didloop = false;}
            } else {
                if (!inStr && (conbuf[concp] == '\'' || conbuf[concp] == '#')) comment = true;
                if (!inStr && conbuf[concp] == '\n') comment = false;
                if (conbuf[concp] == '"') {inStr = !inStr; cmdl++;} else
                if ((conbuf[concp] == ':' && !inStr) || conbuf[concp] == 0) {
                    while (conbuf[concp - cmdl] == ' ' && cmdl > 0) {cmdl--;}
                    cmd = (char*)realloc(cmd, cmdl + 1);
                    cmdpos = concp - cmdl;
                    copyStrSnip(conbuf, concp - cmdl, concp, cmd);
                    cmdl = 0;
                    runcmd();
                    if (cmdint) {txtqunlock(); cmdint = false; goto brkproccmd;}
                    if (concp == -1) goto brkproccmd;
                    if (concp > -1 && conbuf[concp] == 0) {
                        goto brkproccmd;
                    }
                    if (chkinProg) goto fchkint;
                } else
                {cmdl++;}
                if (!didloop) {if (comment) {conbuf[concp] = 0;} concp++;} else {didloop = false;}
            }
        }
        brkproccmd:;
        #ifdef __unix__
        setsig(SIGINT, cleanExit);
        #endif
        txtqunlock();
    }
    txtqunlock();
    cleanExit();
    return 0;
}

static inline uint64_t usTime() {
    gettimeofday(&time1, NULL);
    return time1.tv_sec * 1000000 + time1.tv_usec;
}

static inline uint64_t timer() {
    return usTime() - tval;
}

static inline void resetTimer() {
    tval = usTime();
}

static inline void cb_wait(uint64_t d) {
    #ifdef __unix__
    struct timespec dts;
    dts.tv_sec = d / 1000000;
    dts.tv_nsec = (d % 1000000) * 1000;
    nanosleep(&dts, NULL);
    #else
    uint64_t t = d + usTime();
    while (t > usTime() && !cmdint) {}
    #endif
}

static inline bool isFile(char* path) {
    struct stat pathstat;
    stat(path, &pathstat);
    return !(S_ISDIR(pathstat.st_mode));
}

#ifndef _WIN32

int gcpret, gcpi;
__sighandler_t gcpoldsigh;
bool gcpint = false;

void gcpsigh() {
    puts("\nOOOOOF\n\n");
    setsig(SIGINT, gcpsigh);
    gcpoldsigh(0);
    gcpint = true;
    if (gcpret < gcpi + 1) getCurPos();
    txtqunlock();
    return;
}

#endif

static inline void getCurPos() {
    fflush(stdout);
    cury = 0; curx = 0;
    #ifndef _WIN32
    char buf[16];
    register int i;
    nokill();
    i = kbhit();
    while (i > 0) {getchar(); i--;}
    if (!textlock) {
        sneaktextlock = true;
        tcgetattr(0, &term);
        tcgetattr(0, &restore);
        term.c_lflag &= ~(ICANON|ECHO);
        tcsetattr(0, TCSANOW, &term);
    }
    fputs("\e[6n", stdout);
    fflush(stdout);
    while (!(gcpi += kbhit())) {}
    gcpret = read(1, &buf, gcpi + 1);
    if (!textlock) {
        tcsetattr(0, TCSANOW, &restore);
        sneaktextlock = false;
    }
    yeskill();
    if (!gcpret) return;
    if (cmdint) {cmdint = false; return;}
    sscanf(buf, "\e[%d;%dR", &cury, &curx);
    #else
    CONSOLE_SCREEN_BUFFER_INFO con;
    GetConsoleScreenBufferInfo(hConsole, &con);
    curx = con.dwCursorPosition.X + 1;
    cury = con.dwCursorPosition.Y + 1;
    #endif
}

void unloadProg() {
    if (progindex > 0) progfnstr = progfn[progindex - 1];
    free(progbuf[progindex]);
    free(progfn[progindex]);
    progbuf[progindex] = NULL;
    progfn = (char**)realloc(progfn, progindex * sizeof(char*));
    progbuf = (char**)realloc(progbuf, progindex * sizeof(char*));
    cp = progcp[progindex];
    cmdl = progcmdl[progindex];
    progLine = proglinebuf[progindex];
    progcp = (int64_t*)realloc(progcp, progindex * sizeof(int64_t));
    progcmdl = (int*)realloc(progcmdl, progindex * sizeof(int));
    proglinebuf = (int*)realloc(proglinebuf, progindex * sizeof(int));
    progindex--;
    if (progindex < 0) inProg = false;
}

void unloadAllProg() {
    for (int i = 0; i < progindex; ++i) {
        free(progbuf[i]);
        free(progfn[i]);
    }
    progindex = -1;
    progcp = (int64_t*)realloc(progcp, (progindex + 1) * sizeof(int64_t));
    progcmdl = (int*)realloc(progcmdl, (progindex + 1) * sizeof(int));
    proglinebuf = (int*)realloc(proglinebuf, (progindex + 1) * sizeof(int));
    progbuf = (char**)realloc(progbuf, (progindex + 1) * sizeof(char*));
    progfn = (char**)realloc(progfn, (progindex + 1) * sizeof(char*));
    inProg = false;
}

bool loadProg(char* filename) {
    printf("Loading...");
    fflush(stdout);
    seterrstr(filename);
    cerr = 27;
    FILE* prog = fopen(filename, "r");
    if (!prog) {
        #ifndef _WIN_NO_VT
        fputs("\e[2K", stdout);
        #else
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        int ret = GetConsoleScreenBufferInfo(hConsole, &csbi);
        getCurPos();
        DWORD ret2;
        ret = FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X, (COORD){0, cury - 1}, &ret2);
        (void)ret;
        (void)ret2;
        #endif
        putchar('\r');
        if (errno == ENOENT) cerr = 15;
        return false;
    }
    if (!isFile(filename)) {
        fclose(prog);
        cerr = 18;
        #ifndef _WIN_NO_VT
        fputs("\e[2K", stdout);
        #else
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        int ret = GetConsoleScreenBufferInfo(hConsole, &csbi);
        getCurPos();
        DWORD ret2;
        ret = FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X, (COORD){0, cury - 1}, &ret2);
        (void)ret;
        (void)ret2;
        #endif
        putchar('\r');
        return false;
    }
    progindex++;
    fseek(prog, 0, SEEK_END);
    printf("\b\b\b   \b\b");
    fflush(stdout);
    progfn = (char**)realloc(progfn, (progindex + 1) * sizeof(char*));
    #ifdef _WIN32
    progfn[progindex] = _fullpath(NULL, filename, CB_BUF_SIZE);
    #else
    progfn[progindex] = realpath(filename, NULL);
    #endif
    progfnstr = progfn[progindex];
    progbuf = (char**)realloc(progbuf, (progindex + 1) * sizeof(char*));
    progcp = (int64_t*)realloc(progcp, (progindex + 1) * sizeof(int64_t));
    progcmdl = (int*)realloc(progcmdl, (progindex + 1) * sizeof(int));
    proglinebuf = (int*)realloc(proglinebuf, (progindex + 1) * sizeof(int));
    progcp[progindex] = cp;
    progcmdl[progindex] = cmdl;
    proglinebuf[progindex] = progLine;
    cp = 0;
    cmdl = 0;
    progLine = 1;
    getCurPos();
    int tmpx = curx, tmpy = cury;
    uint32_t fsize = (uint32_t)ftell(prog);
    uint64_t time2 = usTime();
    fseek(prog, 0, SEEK_SET);
    #ifndef _WIN_NO_VT
    printf("\e[%d;%dH", tmpy, tmpx);
    #else
    SetConsoleCursorPosition(hConsole, (COORD){tmpx - 1, tmpy - 1});
    #endif
    printf("(%llu bytes)...", (long long unsigned)fsize);
    fflush(stdout);
    progbuf[progindex] = (char*)malloc(fsize + 1);
    
    int64_t j = 0;
    bool comment = false;
    bool inStr = false;
    #ifndef _WIN_NO_VT
    printf("\e[%d;%dH", tmpy, tmpx);
    #else
    SetConsoleCursorPosition(hConsole, (COORD){tmpx - 1, tmpy - 1});
    #endif
    printf("(0/%llu bytes)...", (long long unsigned)fsize);
    fflush(stdout);
    while (!feof(prog)) {
        int tmpc = fgetc(prog);
        if (tmpc == '"') inStr = !inStr;
        if (!inStr && (tmpc == '\'' || tmpc == '#')) comment = true;
        if (tmpc == '\n') comment = false;
        if (tmpc == '\r' || tmpc == '\t') tmpc = ' ';
        if (!comment) {progbuf[progindex][j] = (char)tmpc; j++;}
        if (usTime() - time2 >= 250000) {
            time2 = usTime();
            #ifndef _WIN_NO_VT
            printf("\e[%d;%dH", tmpy, tmpx);
            #else
            SetConsoleCursorPosition(hConsole, (COORD){tmpx - 1, tmpy - 1});
            #endif
            printf("(%lld/%lld bytes)...", (long long unsigned)ftell(prog), (long long unsigned)fsize);
            fflush(stdout);
        }
    }
    #ifndef _WIN_NO_VT
    printf("\e[%d;%dH", tmpy, tmpx);
    #else
    SetConsoleCursorPosition(hConsole, (COORD){tmpx - 1, tmpy - 1});
    #endif
    printf("(%lld/%lld bytes)...", (long long unsigned)ftell(prog), (long long unsigned)fsize);
    #ifndef _WIN_NO_VT
    fputs("\e[2K", stdout);
    fflush(stdout);
    #else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int ret = GetConsoleScreenBufferInfo(hConsole, &csbi);
    getCurPos();
    DWORD ret2;
    ret = FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X, (COORD){0, cury - 1}, &ret2);
    (void)ret;
    (void)ret2;
    #endif
    putchar('\r');
    fflush(stdout);
    if (j < 1) j = 1;
    progbuf[progindex][j - 1] = 0;
    fclose(prog);
    return true;
}

static inline double randNum(double num1, double num2) {
    double range = num2 - num1;
    double div = RAND_MAX / range;
    return num1 + (rand() / div);
}

char* chkCmdPtr = NULL;

static inline bool chkCmd(int ct, ...) {
    va_list args;
    va_start(args, ct);
    #ifdef BUILT_IN_COMMAND_COMPARE
    for (int32_t i = 0; i < ct; ++i) {
        char* str2 = va_arg(args, char*);
        if (!strcmp(chkCmdPtr, str2)) return true;
    }
    return false;
    #else
    bool match = false;
    for (int i = 0; i < ct; ++i) {
        char* str1 = chkCmdPtr;
        char* str2 = va_arg(args, char*);
        while (1) {
            if (!*str1 && !*str2) break;
            if (*str1 != *str2) goto nmatch;
            str1++;
            str2++;
        }
        match = true;
        nmatch:;
        if (match) return true;
    }
    return false;
    #endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
static inline bool isSpChar(char c) {
    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '^':
            return true;
            break;
        default:
            return false;
            break;
    }
}

static inline bool isValidVarChar(char c) {
    switch (c) {
        case 'A' ... 'Z':
        case '0' ... '9':
        case '#':
        case '$':
        case '%':
        case '!':
        case '?':
        case '@':
        case '[':
        case ']':
        case '_':
            return true;
            break;
        default:
            return false;
            break;
    }
}

static inline bool isValidHexChar(char c) {
    switch (c) {
        case 'a' ... 'f':
        case 'A' ... 'F':
        case '0' ... '9':
            return true;
            break;
        default:
            return false;
            break;
    }
}
#pragma GCC diagnostic pop

#ifndef BUILT_IN_STRING_FUNCS
static inline void copyStr(char* str1, char* str2) {
    int32_t i;
    for (i = 0; str1[i]; ++i) {str2[i] = str1[i];}
    str2[i] = 0;
}

static inline void copyStrApnd(char* str1, char* str2) {
    int32_t j = 0, i = 0;
    for (i = strlen(str2); str1[j]; ++i) {str2[i] = str1[j]; ++j;}
    str2[i] = 0;
}
#endif

static inline void copyStrSnip(char* str1, int32_t i, int32_t j, char* str2) {
    int32_t i2 = 0;
    for (; i < j && str1[i]; ++i) {str2[i2] = str1[i]; ++i2;}
    str2[i2] = 0;
}

static inline void copyStrTo(char* str1, int32_t i, char* str2) {
    int32_t i2 = 0;
    int32_t i3;
    for (i3 = i; str1[i]; ++i) {str2[i3] = str1[i2]; ++i2; ++i3;}
    str2[i3] = 0;
}

static inline void strApndChar(char* str, char c) {
    int32_t len = 0;
    while (str[len]) {len++;}
    str[len] = c;
    len++;
    str[len] = 0;
}

static inline void upCase(char* str) {
    for (int32_t i = 0; str[i]; ++i) {
        if (str[i] >= 'a' && str[i] <= 'z') str[i] -= 32;
    }
}

static inline void lowCase(char* str) {
    for (int32_t i = 0; str[i]; ++i) {
        if (str[i] >= 'A' && str[i] <= 'Z') str[i] += 32;
    }
}

static inline void seterrstr(char* newstr) {
    size_t nslen = strlen(newstr);
    if (!errstr || strlen(errstr) != nslen) errstr = (char*)realloc(errstr, nslen + 1);
    copyStr(newstr, errstr);
}

void updateTxtAttrib() {
    #ifndef _WIN_NO_VT
    fputs("\e[0m", stdout);
    if (txt_fgc) printf("\e[38;5;%um", fgc);
    if (txt_bgc) printf("\e[48;5;%um", bgc);
    if (txt_bold) fputs("\e[1m", stdout);
    if (txt_italic) fputs("\e[3m", stdout);
    if (txt_underln) fputs("\e[4m", stdout);
    if (txt_underlndbl) fputs("\e[21m", stdout);
    if (txt_underlnsqg) fputs("\e[4:3m", stdout);
    if (txt_strike) fputs("\e[9m", stdout);
    if (txt_overln) fputs("\e[53m", stdout);
    if (txt_dim) fputs("\e[2m", stdout);
    if (txt_blink) fputs("\e[5m", stdout);
    if (txt_hidden) fputs("\e[8m", stdout);
    if (txt_reverse) fputs("\e[7m", stdout);
    if (txt_underlncolor) printf("\e[58:5:%um", txt_underlncolor);
    #else
    uint8_t tfgc, tbgc;
    switch (fgc % 16) {
        case 1: tfgc = 4; break;
        case 3: tfgc = 6; break;
        case 4: tfgc = 1; break;
        case 6: tfgc = 3; break;
        case 9: tfgc = 12; break;
        case 11: tfgc = 14; break;
        case 12: tfgc = 9; break;
        case 14: tfgc = 11; break;
        default: tfgc = fgc; break;
    }
    switch (bgc % 16) {
        case 1: tbgc = 4; break;
        case 3: tbgc = 6; break;
        case 4: tbgc = 1; break;
        case 6: tbgc = 3; break;
        case 9: tbgc = 12; break;
        case 11: tbgc = 14; break;
        case 12: tbgc = 9; break;
        case 14: tbgc = 11; break;
        default: tbgc = bgc; break;
    }
	SetConsoleTextAttribute(hConsole, (tfgc % 16) + (tbgc % 16) * 16);
    #endif
    fflush(stdout);
}

char buf[CB_BUF_SIZE];

void getStr(char* str1, char* str2) {
    int32_t j = 0, i;
    for (i = 0; str1[i]; ++i) {
        char c = str1[i];
        if (c == '\\') {
            ++i;
            char h[5];
            unsigned int tc = 0;
            switch (str1[i]) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 'f': c = '\f'; break;
                case 't': c = '\t'; break;
                case 'v': c = '\v'; break;
                case 'b': c = '\b'; break;
                case 'e': c = '\e'; break;
                case 'x':
                    h[0] = '0';
                    h[1] = 'x';
                    if (!isValidHexChar(str1[++i])) {i -= 2; break;}
                    h[2] = str1[i];
                    if (!isValidHexChar(str1[++i])) {i -= 3; break;}
                    h[3] = str1[i];
                    h[4] = 0;
                    sscanf(h, "%x", &tc);
                    c = tc;
                    if (!c) ++j;
                    break;
                case '\\': c = '\\'; break;
                default: --i; break;
            }
        }
        buf[j] = c;
        ++j;
    }
    buf[j] = 0;
    copyStr(buf, str2);
}

uint8_t getType(char* str) {
    if (str[0] == '"') {if (str[strlen(str) - 1] != '"') {return 0;} return 1;}
    bool p = false;
    for (int32_t i = 0; str[i]; ++i) {
        if (str[i] == '-') {} else
        if ((str[i] < '0' || str[i] > '9') && str[i] != '.') {return 255;} else
        if (str[i] == '.') {if (p) {return 0;} p = true;}
    }
    return 2;
}

static inline int getArg(int, char*, char*);
static inline int getArgCt(char*);

uint16_t getFuncIndex = 0;
char* getFunc_gftmp[2] = {NULL, NULL};

uint8_t getFunc(char* inbuf, char* outbuf) {
    char** farg;
    uint8_t* fargt;
    int* flen;
    int fargct;
    int ftmpct = 0;
    int ftype = 0;
    char* gftmp[2];
    if (getFuncIndex) {
        gftmp[0] = malloc(CB_BUF_SIZE);
        gftmp[1] = malloc(CB_BUF_SIZE);
    } else {
        gftmp[0] = getFunc_gftmp[0];
        gftmp[1] = getFunc_gftmp[1];
    }
    ++getFuncIndex;
    int32_t i;
    for (i = 0; inbuf[i] != '('; ++i) {if (inbuf[i] >= 'a' && inbuf[i] <= 'z') inbuf[i] = inbuf[i] - 32;}
    copyStrSnip(inbuf, i + 1, strlen(inbuf) - 1, gftmp[0]);
    fargct = getArgCt(gftmp[0]);
    farg = malloc((fargct + 1) * sizeof(char*));
    flen = malloc((fargct + 1) * sizeof(int));
    fargt = malloc((fargct + 1) * sizeof(uint8_t));
    for (int j = 0; j <= fargct; ++j) {
        if (j == 0) {
            flen[0] = i;
            farg[0] = (char*)malloc(flen[0] + 1);
            copyStrSnip(inbuf, 0, i, farg[0]);
        } else {
            getArg(j - 1, gftmp[0], gftmp[1]);
            fargt[j] = getVal(gftmp[1], gftmp[1]);
            if (fargt[j] == 0) goto fnoerrscan;
            if (fargt[j] == 255) fargt[j] = 0;
            flen[j] = strlen(gftmp[1]);
            farg[j] = (char*)malloc(flen[j] + 1);
            copyStr(gftmp[1], farg[j]);
            ftmpct++;
        }
    }
    outbuf[0] = 0;
    cerr = 127;
    chkCmdPtr = farg[0];
    #include "functions.c"
    fexit:;
    if (cerr > 124 && cerr < 128) seterrstr(farg[0]);
    fnoerrscan:;
    for (int j = 0; j <= ftmpct; ++j) {
        free(farg[j]);
    }
    free(farg);
    free(flen);
    free(fargt);
    --getFuncIndex;
    if (getFuncIndex) {free(gftmp[0]); free(gftmp[1]);}
    if (cerr) return 0;
    return ftype;
}

uint8_t getVar(char* vn, char* varout) {
    int32_t vnlen = strlen(vn);
    if (vn[vnlen - 1] == ')') {
        return getFunc(vn, varout);
    }
    upCase(vn);
    if (!vn[0] || vn[0] == '[' || vn[0] == ']') {
        cerr = 4;
        seterrstr(vn);
        return 0;
    }
    if (getType(vn) != 255) {
        cerr = 4;
        seterrstr(vn);
        return 0;
    }
    bool isArray = false;
    int32_t aindex = 0;
    for (register int32_t i = 0; vn[i]; ++i) {
        if (!isValidVarChar(vn[i])) {
            cerr = 4;
            seterrstr(vn);
            return 0;
        }
        if (vn[i] == ']') {
            cerr = 1;
            return 0;
        }
        if (vn[i] == '[') {
            if (vn[vnlen - 1] != ']') {cerr = 1; return 0;}
            copyStrSnip(vn, i + 1, vnlen - 1, gpbuf);
            if (!gpbuf[0]) {cerr = 1; return 0;}
            cerr = 2;
            uint8_t tmpt = getVal(gpbuf, gpbuf);
            if (tmpt != 2) {return 0;}
            cerr = 0;
            aindex = atoi(gpbuf);
            vn[i] = 0;
            vnlen = strlen(vn);
            isArray = true;
            break;
        }
    }
    int v = -1;
    for (register int i = 0; i < varmaxct; ++i) {
        if (varinuse[i]) {if (!strcmp(vn, varname[i])) {v = i; break;}}
    }
    if (v == -1) {
        if (isArray) {
            cerr = 23;
            seterrstr(vn);
            return 0;
        }
        if (vn[vnlen - 1] == '$') {varout[0] = 0; return 1;}
        else {varout[0] = '0'; varout[1] = 0; return 2;}
    } else {
        if (vardata[v].size == -1) {
            if (isArray) {
                cerr = 23;
                seterrstr(vn);
                return 0;
            }
        } else {
            if (!isArray) {
                cerr = 24;
                seterrstr(vn);
                return 0;
            }
            if (aindex < 0 || aindex > vardata[v].size) {
                cerr = 22;
                sprintf(gpbuf, "%s[%li]", vn, (long int)aindex);
                seterrstr(gpbuf);
                return 0;
            }
        }
        copyStr(vardata[v].data[aindex], varout);
        return vardata[v].type;
    }
    return 0;
}

bool delVar(char*);

bool setVar(char* vn, char* val, uint8_t t, int32_t s) {
    int32_t vnlen = strlen(vn);
    upCase(vn);
    if (!vn[0] || vn[0] == '[' || vn[0] == ']') {
        cerr = 4;
        seterrstr(vn);
        return false;
    }
    if (getType(vn) != 255) {
        cerr = 4;
        seterrstr(vn);
        return false;
    }
    bool isArray = false;
    int32_t aindex = 0;
    for (register int32_t i = 0; vn[i]; ++i) {
        if (!isValidVarChar(vn[i])) {
            cerr = 4;
            seterrstr(vn);
            return false;
        }
        if (vn[i] == ']') {
            cerr = 1;
            return 0;
        }
        if (vn[i] == '[') {
            if (vn[vnlen - 1] != ']') {cerr = 1; return 0;}
            if (s != -1) {cerr = 4; seterrstr(vn); return 0;}
            copyStrSnip(vn, i + 1, vnlen - 1, gpbuf);
            if (!gpbuf[0]) {cerr = 1; return 0;}
            cerr = 2;
            uint8_t tmpt = getVal(gpbuf, gpbuf);
            if (tmpt != 2) {return 0;}
            cerr = 0;
            aindex = atoi(gpbuf);
            vn[i] = 0;
            vnlen = strlen(vn);
            isArray = true;
            break;
        }
    }
    int v = -1;
    for (register int i = 0; i < varmaxct; ++i) {
        if (!strcmp(vn, varname[i])) {v = i; break;}
    }
    if (v == -1) {
        bool incmaxct = true;
        if (isArray) {
            cerr = 23;
            seterrstr(vn);
            return false;
        }
        for (register int i = 0; i < varmaxct; ++i) {
            if (!varinuse[i]) {v = i; incmaxct = false; break;}
        }
        if (v == -1) v = varmaxct;
        if (incmaxct) {
            varmaxct++;
            varname = (char**)realloc(varname, varmaxct * sizeof(char*));
            vardata = (cb_var*)realloc(vardata, varmaxct * sizeof(cb_var));
            varinuse = (bool*)realloc(varinuse, varmaxct * sizeof(bool));
        }
        varinuse[v] = true;
        varname[v] = (char*)malloc(vnlen + 1);
        copyStr(vn, varname[v]);
        vardata[v].size = s;
        vardata[v].type = t;
        if (s == -1) s = 0;
        vardata[v].data = (char**)malloc((s + 1) * sizeof(char*));
        for (int32_t i = 0; i <= s; ++i) {
            vardata[v].data[i] = (char*)malloc(strlen(val) + 1);
            if (!vardata[v].data[i]) {
                delVar(vn);
                cerr = 26;
                return false;
            }
            copyStr(val, vardata[v].data[i]);
        }
    } else {
        if (s != -1) {cerr = 25; return false;}
        if (t != vardata[v].type) {cerr = 2; return false;}
        if (isArray && (aindex < 0 || aindex > vardata[v].size)) {
            cerr = 22;
            sprintf(gpbuf, "%s[%li]", vn, (long int)aindex);
            seterrstr(gpbuf);
            return 0;
        }
        vardata[v].data[aindex] = (char*)realloc(vardata[v].data[aindex], strlen(val) + 1);
        copyStr(val, vardata[v].data[aindex]);
    }
    return true;
}

bool delVar(char* vn) {
    upCase(vn);
    if (!vn[0] || vn[0] == '[' || vn[0] == ']') {
        cerr = 4;
        seterrstr(vn);
        return false;
    }
    if (getType(vn) != 255) {
        cerr = 4;
        seterrstr(vn);
        return false;
    }
    for (register int32_t i = 0; vn[i]; ++i) {
        if (!isValidVarChar(vn[i])) {
            cerr = 4;
            seterrstr(vn);
            return false;
        }
        if (vn[i] == '[') {
            cerr = 4;
            seterrstr(vn);
            return false;
        }
    }
    int v = -1;
    for (register int i = 0; i < varmaxct; ++i) {
        if (!strcmp(vn, varname[i])) {v = i; break;}
    }
    if (v != -1) {
        varinuse[v] = false;
        free(varname[v]);
        for (int32_t i = 0; i <= vardata[v].size; ++i) {
            free(vardata[v].data[i]);
        }
        free(vardata[v].data);
        if (v == varmaxct - 1) {
            while (!varinuse[v] && v >= 0) {varmaxct--; v--;}
            varname = (char**)realloc(varname, varmaxct * sizeof(char*));
            vardata = (cb_var*)realloc(vardata, varmaxct * sizeof(cb_var));
            varinuse = (bool*)realloc(varinuse, varmaxct * sizeof(bool));
        }
    }
    return true;
}

static inline bool gvchkchar(char* tmp, int32_t i) {
    if (isSpChar(tmp[i + 1])) {
        if (tmp[i + 1] == '-') {
            if (isSpChar(tmp[i + 2])) {
                cerr = 1; return false;
            }
        } else {
            cerr = 1; return false;
        }
    } else {
        if (isSpChar(tmp[i - 1])) {
            cerr = 1; return false;
        }
    }
    return true;
}

uint16_t getValIndex = 0;
char* getVal_tmp[4] = {NULL, NULL, NULL, NULL};

uint8_t getVal(char* inbuf, char* outbuf) {
    if (inbuf[0] == 0) {return 255;}
    char* tmp[4];
    if (getValIndex) {
        tmp[0] = malloc(CB_BUF_SIZE);
        tmp[1] = malloc(CB_BUF_SIZE);
        tmp[2] = malloc(CB_BUF_SIZE);
        tmp[3] = malloc(CB_BUF_SIZE);
    } else {
        tmp[0] = getVal_tmp[0];
        tmp[1] = getVal_tmp[1];
        tmp[2] = getVal_tmp[2];
        tmp[3] = getVal_tmp[3];
    }
    getValIndex++;
    int32_t ip = 0, jp = 0;
    uint8_t t = 0;
    uint8_t dt = 0;
    bool inStr = false;
    register double num1 = 0;
    register double num2 = 0;
    register double num3 = 0;
    int numAct;
    bool* seenStr = NULL;
    if ((isSpChar(inbuf[0]) && inbuf[0] != '-') || isSpChar(inbuf[strlen(inbuf) - 1])) {cerr = 1; dt = 0; goto gvreturn;}
    int pct = 0, bct = 0;
    tmp[0][0] = 0; tmp[1][0] = 0; tmp[2][0] = 0; tmp[3][0] = '"'; tmp[3][1] = 0;
    seenStr = malloc(sizeof(bool));
    seenStr[0] = false;
    for (register int32_t i = 0; inbuf[i]; ++i) {
        switch (inbuf[i]) {
            default:
                if (inStr) break;
                if (inbuf[i] == ',' || isSpChar(inbuf[i])) seenStr[pct] = false;
                break;
            case '"':
                inStr = !inStr;
                if (inStr && seenStr[pct]) {
                    dt = 0;
                    cerr = 1;
                    goto gvreturn;
                }
                seenStr[pct] = true;
                break;
            case '(':
                if (inStr) break;
                if (pct == 0) {ip = i;}
                pct++;
                seenStr = (bool*)realloc(seenStr, (pct + 1) * sizeof(bool));
                seenStr[pct] = false;
                break;
            case ')':
                if (inStr) break;
                pct--;
                seenStr = (bool*)realloc(seenStr, (pct + 1) * sizeof(bool));
                if (pct == 0 && (ip == 0 || isSpChar(inbuf[ip - 1]))) {
                    int32_t tmplen[2];
                    tmplen[0] = strlen(inbuf);
                    jp = i;
                    copyStrSnip(inbuf, ip + 1, jp, tmp[0]);
                    t = getVal(tmp[0], tmp[0]);
                    if (t == 0) {dt = 0; goto gvreturn;}
                    if (dt == 0) dt = t;
                    if (t == 255) {t = 1; dt = 1;}
                    if (t != dt) {cerr = 2; dt = 0; goto gvreturn;}
                    copyStrSnip(inbuf, jp + 1, strlen(inbuf), tmp[1]);
                    inbuf[ip] = 0;
                    if (t == 1) copyStrApnd(tmp[3], inbuf);
                    copyStrApnd(tmp[0], inbuf);
                    if (t == 1) copyStrApnd(tmp[3], inbuf);
                    copyStrApnd(tmp[1], inbuf);
                    tmp[0][0] = 0; tmp[1][0] = 0; tmp[2][0] = 0; tmp[3][0] = 0;
                    tmplen[1] = strlen(inbuf);
                    i -= tmplen[0] - tmplen[1];
                }
                break;
            case '[':
                if (inStr) break;
                bct++;
                break;
            case ']':
                if (inStr) break;
                bct--;
                break;
        }
    }
    if (pct || bct) {cerr = 1; dt = 0; goto gvreturn;}
    ip = 0; jp = 0;
    tmp[0][0] = 0; tmp[1][0] = 0; tmp[2][0] = 0; tmp[3][0] = 0;
    while (1) {
        pct = 0;
        bct = 0;
        while (inbuf[jp]) {
            if (inbuf[jp] == '"') inStr = !inStr;
            if (!inStr) {
                switch (inbuf[jp]) {
                    case '(': pct++; break;
                    case ')': pct--; break;
                    case '[': bct++; break;
                    case ']': bct--; break;
                    default:
                        if (isSpChar(inbuf[jp]) && !pct && !bct) goto gvwhileexit1;
                        break;
                }
            }
            jp++;
        }
        gvwhileexit1:;
        if (inStr) {dt = 0; cerr = 1; goto gvreturn;}
        copyStrSnip(inbuf, ip, jp, tmp[0]);
        t = getType(tmp[0]);
        if (t == 1) getStr(tmp[0], tmp[0]);
        if (t == 255) {
            t = getVar(tmp[0], tmp[0]);
            if (t == 0) {dt = 0; dt = 0; goto gvreturn;}
            if (t == 1) {
                tmp[2][0] = '"'; tmp[2][1] = 0;
                copyStr(tmp[2], tmp[3]);
                swap(tmp[0], tmp[3]);
                copyStrApnd(tmp[3], tmp[0]);
                copyStrApnd(tmp[2], tmp[0]);
            }
        }
        if (t && dt == 0) {dt = t;} else
        if ((t && t != dt)) {cerr = 2; dt = 0; goto gvreturn;} else
        if (t == 0) {cerr = 1; dt = 0; goto gvreturn;}
        if ((dt == 1 && inbuf[jp] != '+') && inbuf[jp]) {cerr = 1; dt = 0; goto gvreturn;}
        if (t == 1) {copyStrSnip(tmp[0], 1, strlen(tmp[0]) - 1, tmp[2]); copyStrApnd(tmp[2], tmp[1]);} else
        if (t == 2) {
            copyStrSnip(inbuf, jp, strlen(inbuf), tmp[1]);
            copyStrApnd(tmp[1], tmp[0]);
            register int32_t p1, p2, p3;
            bool inStr = false;
            pct = 0;
            bct = 0;
            while (true) {
                numAct = 0;
                p1 = 0, p2 = 0, p3 = 0;
                for (register int32_t i = 0; tmp[0][i]; ++i) {
                    if (tmp[0][i] == '"') inStr = !inStr;
                    if (!inStr) {
                        switch (tmp[0][i]) {
                            case '(': pct++; break;
                            case ')': pct--; break;
                            case '[': bct++; break;
                            case ']': bct--; break;
                            case '^':
                                if (!pct && !bct) {
                                    if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                    p2 = i;
                                    numAct = 4;
                                    goto gvsetact;
                                }
                                break;
                        }
                    }
                }
                for (register int32_t i = 0; tmp[0][i]; ++i) {
                    if (tmp[0][i] == '"') inStr = !inStr;
                    if (!inStr) {
                        switch (tmp[0][i]) {
                            case '(': pct++; break;
                            case ')': pct--; break;
                            case '[': bct++; break;
                            case ']': bct--; break;
                            case '*':
                                if (!pct && !bct) {
                                    if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                    p2 = i;
                                    numAct = 2;
                                    goto gvsetact;
                                }
                                break;
                            case '/':
                                if (!pct && !bct) {
                                    if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                    p2 = i;
                                    numAct = 3;
                                    goto gvsetact;
                                }
                                break;
                        }
                    }
                }
                for (register int32_t i = 0; tmp[0][i]; ++i) {
                    if (tmp[0][i] == '"') inStr = !inStr;
                    if (!inStr) {
                        switch (tmp[0][i]) {
                            case '(': pct++; break;
                            case ')': pct--; break;
                            case '[': bct++; break;
                            case ']': bct--; break;
                            case '+':
                                if (!pct && !bct) {
                                    if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                    p2 = i;
                                    numAct = 0;
                                    goto gvsetact;
                                }
                                break;
                            case '-':
                                if (!pct && !bct) {
                                    if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                    p2 = i;
                                    numAct = 1;
                                    goto gvsetact;
                                }
                                break;
                        }
                    }
                }
                gvsetact:;
                inStr = false;
                pct = 0;
                bct = 0;
                if (p2 == 0) {
                    if (p3 == 0) {
                        t = getType(tmp[0]);
                        if (t == 0) {cerr = 1; dt = 0; goto gvreturn;} else
                        if (t == 255) {
                            t = getVar(tmp[0], tmp[0]);
                            if (t == 0) {dt = 0; goto gvreturn;}
                            if (t == 255) {t = 2; tmp[0][0] = '0'; tmp[0][1] = 0;}
                            if (t != 2) {cerr = 2; dt = 0; goto gvreturn;}
                        }
                    }
                    swap(tmp[0], tmp[1]);
                    goto gvfexit;
                }
                tmp[1][0] = 0; tmp[2][0] = 0; tmp[3][0] = 0;
                for (register int32_t i = p2 - 1; i > 0; --i) {
                    if (tmp[0][i] == '"') inStr = !inStr;
                    if (!inStr) {
                        switch (tmp[0][i]) {
                            case '(': pct++; break;
                            case ')': pct--; break;
                            case '[': bct++; break;
                            case ']': bct--; break;
                            default:
                                if (isSpChar(tmp[0][i]) && !inStr && !pct && !bct) {p1 = i; goto gvforexit1;}
                                break;
                        }
                    }
                }
                gvforexit1:;
                for (register int32_t i = p2 + 1; true; ++i) {
                    if (tmp[0][i] == '"') inStr = !inStr;
                    if (!inStr) {
                        switch (tmp[0][i]) {
                            case '(': pct++; break;
                            case ')': pct--; break;
                            case '[': bct++; break;
                            case ']': bct--; break;
                            case 0: p3 = i; goto gvforexit2;
                            default:
                                if (isSpChar(tmp[0][i]) && i != p2 + 1 && !pct && !bct) {p3 = i; goto gvforexit2;}
                                break;
                        }
                    }
                }
                gvforexit2:;
                if (tmp[0][p1] == '+') p1++;
                copyStrSnip(tmp[0], p1, p2, tmp[2]);
                t = getType(tmp[2]);
                if (t == 0) {cerr = 1; dt = 0; goto gvreturn;} else
                if (t == 255) {t = getVar(tmp[2], tmp[2]); if (t == 0) {dt = 0; goto gvreturn;} if (t != 2) {cerr = 2; dt = 0; goto gvreturn;}}
                copyStrSnip(tmp[0], p2 + 1, p3, tmp[3]);
                t = getType(tmp[3]);
                if (t == 0) {cerr = 1; dt = 0; goto gvreturn;} else
                if (t == 255) {t = getVar(tmp[3], tmp[3]); if (t == 0) {dt = 0; goto gvreturn;} if (t != 2) {cerr = 2; dt = 0; goto gvreturn;}}
                if (!strcmp(tmp[2], ".")) {cerr = 1; dt = 0; goto gvreturn;}
                num1 = atof(tmp[2]);
                if (!strcmp(tmp[2], ".")) {cerr = 1; dt = 0; goto gvreturn;}
                num2 = atof(tmp[3]);
                switch (numAct) {
                    case 0: num3 = num1 + num2; break;
                    case 1: num3 = num1 - num2; break;
                    case 2: num3 = num1 * num2; break;
                    case 3: if (num2 == 0) {cerr = 5; dt = 0; goto gvreturn;} num3 = num1 / num2; break;
                    case 4:
                        if (num1 == 0) {if (num2 == 0) {cerr = 5; dt = 0; goto gvreturn;} num3 = 0; break;}
                        if (num2 == 0) {num3 = 1; break;}
                        num3 = pow(num1, num2);
                        if (num1 < 0 && num3 > 0) {num3 *= -1;}
                        break;
                }
                tmp[1][0] = 0;
                if (num3 == -0 || (num3 < 0 && num3 > -0.0000005)) {
                    tmp[2][0] = '0';
                    tmp[2][1] = 0;
                    goto skipstrchk;
                }
                if (num3 < -0.0000004 && num3 > -0.0000009) {
                    copyStr("-0.000001", tmp[0]);
                    goto skipstrchk;
                }
                sprintf(tmp[2], "%lf", num3);
                skipstrchk:;
                int32_t i = 0;
                while (tmp[2][i] == '0') i++;
                copyStrSnip(tmp[2], i, strlen(tmp[2]), tmp[2]);
                copyStrSnip(tmp[0], p3, strlen(tmp[0]), tmp[3]);
                if (p1) copyStrSnip(tmp[0], 0, p1, tmp[1]);
                copyStrApnd(tmp[2], tmp[1]);
                copyStrApnd(tmp[3], tmp[1]);
                swap(tmp[1], tmp[0]);
            }
        }
        if (inbuf[jp] == 0) {break;}
        jp++;
        ip = jp;
    }
    gvfexit:;
    if (dt == 2) {
        if (tmp[1][0] == '.' && !tmp[1][1]) {cerr = 1; dt = 0; goto gvreturn;}
        int32_t i = 0, j = strlen(tmp[1]);
        bool dp = false;
        while (tmp[1][i]) {if (tmp[1][i] == '.') {tmp[1][i + 7] = 0; dp = true; break;} i++;}
        i = 0;
        while (tmp[1][i] == '0') i++;
        if (dp) {
            j--;
            while (tmp[1][j] == '0') {j--;}
            if (tmp[1][j] == '.') {j--;}
            j++;
        }
        outbuf[0] = 0;
        copyStrSnip(tmp[1], i, j, tmp[2]);
        if (tmp[1][i] == '.') strcpy(outbuf, "0");
        copyStrApnd(tmp[2], outbuf);
    } else {
        copyStr(tmp[1], outbuf);
    }
    if (outbuf[0] == 0 && dt != 1) {outbuf[0] = '0'; outbuf[1] = 0; dt = 2;}
    gvreturn:;
    getValIndex--;
    free(seenStr);
    if (getValIndex) {free(tmp[0]); free(tmp[1]); free(tmp[2]); free(tmp[3]);}
    return dt;
}

char satmpbuf[CB_BUF_SIZE];

bool solvearg(int i) {
    if (i == 0) {
        argt[0] = 0;
        arg[0] = tmpargs[0];
        argl[0] = strlen(arg[0]);
        return true;
    }
    argt[i] = 0;
    satmpbuf[0] = 0;
    argt[i] = getVal(tmpargs[i], satmpbuf);
    arg[i] = (char*)realloc(arg[i], strlen(satmpbuf) + 1);
    copyStr(satmpbuf, arg[i]);
    argl[i] = strlen(arg[i]);
    if (argt[i] == 0) return false;
    if (argt[i] == 255) {argt[i] = 0;}
    return true;
}

static inline int getArgCt(char* inbuf) {
    int ct = 0;
    bool inStr = false;
    int pct = 0, bct = 0;
    int32_t j = 0;
    while (inbuf[j] == ' ') {--j;}
    for (int32_t i = j; inbuf[i]; ++i) {
        if (ct == 0) ct = 1;
        if (inbuf[i] == '(' && !inStr) {pct++;}
        if (inbuf[i] == ')' && !inStr) {pct--;}
        if (inbuf[i] == '[' && !inStr) {bct++;}
        if (inbuf[i] == ']' && !inStr) {bct--;}
        if (inbuf[i] == '"') inStr = !inStr;
        if (inbuf[i] == ',' && !inStr && pct == 0 && bct == 0) ct++;
    }
    return ct;
}

static inline int getArg(int num, char* inbuf, char* outbuf) {
    bool inStr = false;
    int pct = 0, bct = 0;
    int ct = 0;
    int32_t len = 0;
    for (int32_t i = 0; inbuf[i] && ct <= num; ++i) {
        if (inbuf[i] == '(' && !inStr) {pct++;}
        if (inbuf[i] == ')' && !inStr) {pct--;}
        if (inbuf[i] == '[' && !inStr) {bct++;}
        if (inbuf[i] == ']' && !inStr) {bct--;}
        if (inbuf[i] == '"') {inStr = !inStr;}
        if (inbuf[i] == ' ' && !inStr) {} else
        if (inbuf[i] == ',' && !inStr && pct == 0 && bct == 0) {ct++;} else
        if (ct == num) {outbuf[len] = inbuf[i]; len++;}
    }
    outbuf[len] = 0;
    return len;
}

char tmpbuf[2][CB_BUF_SIZE];

void mkargs() {
    int32_t j = 0;
    while (cmd[j] == ' ') {j++;}
    int32_t h = j;
    while (cmd[h] != ' ' && cmd[h] != '=' && cmd[h]) {h++;}
    copyStrSnip(cmd, h + 1, strlen(cmd), tmpbuf[0]);
    int32_t tmph = h;
    while (cmd[tmph] == ' ' && cmd[tmph]) {tmph++;}
    if (cmd[tmph] == '=') {
        strcpy(tmpbuf[1], "SET ");
        cmd[tmph] = ',';
        copyStrApnd(cmd, tmpbuf[1]);
        cmd = (char*)realloc(cmd, strlen(tmpbuf[1]) + 1);
        copyStr(tmpbuf[1], cmd);
        copyStr(tmpbuf[1], tmpbuf[0]);
        tmpbuf[1][0] = 0;
        h = 3;
        j = 0;
    }
    argct = getArgCt(tmpbuf[0]);
    tmpargs = (char**)realloc(tmpargs, (argct + 1) * sizeof(char*));
    argl = (int32_t*)realloc(argl, (argct + 1) * sizeof(int32_t));
    for (int i = 0; i <= argct; ++i) {
        argl[i] = 0;
        if (i == 0) {
            copyStrSnip(cmd, j, h, tmpbuf[0]);
            argl[i] = strlen(tmpbuf[0]);
            tmpargs[i] = malloc(argl[i] + 1);
            copyStr(tmpbuf[0], tmpargs[i]);
            copyStrSnip(cmd, h + 1, strlen(cmd), tmpbuf[0]);
        } else {
            argl[i] = getArg(i - 1, tmpbuf[0], tmpbuf[1]);
            tmpargs[i] = malloc(argl[i] + 1);
            copyStr(tmpbuf[1], tmpargs[i]);
        }
        tmpargs[i][argl[i]] = 0;
    }
    if (argct == 1 && tmpargs[1][0] == 0) {argct = 0;}
    for (int i = 0; i <= argct; ++i) {tmpargs[i][argl[i]] = 0;}
}

int ltIndex = 0;
char lttmp[3][CB_BUF_SIZE];

uint8_t logictest(char* inbuf) {
    int32_t tmpp = 0;
    uint8_t t1 = 0;
    uint8_t t2 = 0;
    int32_t p = 0;
    bool inStr = false;
    int pct = 0, bct = 0;
    ltIndex++;
    while (inbuf[p] == ' ') {p++;}
    if (p >= (int32_t)strlen(inbuf)) {cerr = 10; goto ltreturn;}
    for (int32_t i = p; true; ++i) {
        if (inbuf[i] == '(' && !inStr) {pct++;}
        if (inbuf[i] == ')' && !inStr) {pct--;}
        if (inbuf[i] == '[' && !inStr) {bct++;}
        if (inbuf[i] == ']' && !inStr) {bct--;}
        if (inbuf[i] == '"') {inStr = !inStr;}
        if (inbuf[i] == 0) {cerr = 1; goto ltreturn;}
        if ((inbuf[i] == '<' || inbuf[i] == '=' || inbuf[i] == '>') && !inStr && pct == 0 && bct == 0) {p = i; break;}
        if (inbuf[i] == ' ' && !inStr) {} else
        {lttmp[0][tmpp] = inbuf[i]; tmpp++;}
    }
    lttmp[0][tmpp] = 0;
    tmpp = 0;
    for (int32_t i = p; true; ++i) {
        if (tmpp > 2) {cerr = 1; goto ltreturn;}
        if (inbuf[i] != '<' && inbuf[i] != '=' && inbuf[i] != '>') {p = i; break;} else
        {lttmp[1][tmpp] = inbuf[i]; tmpp++;}
    }
    lttmp[1][tmpp] = 0;
    tmpp = 0;
    for (int32_t i = p; true; ++i) {
        if (inbuf[i] == 0) {break;}
        if (inbuf[i] == ' ' && !inStr) {} else
        {lttmp[2][tmpp] = inbuf[i]; tmpp++;}
    }
    lttmp[2][tmpp] = 0;
    t1 = getVal(lttmp[0], lttmp[0]);
    if (t1 == 0) goto ltreturn;
    t2 = getVal(lttmp[2], lttmp[2]);
    if (t2 == 0) goto ltreturn;
    if (t1 != t2) {cerr = 2; goto ltreturn;}
    if (!strcmp(lttmp[1], "=")) {
        ltIndex--;
        return (uint8_t)(bool)!strcmp(lttmp[0], lttmp[2]);
    } else if (!strcmp(lttmp[1], "<>")) {
        ltIndex--;
        return (uint8_t)(bool)strcmp(lttmp[0], lttmp[2]);
    } else if (!strcmp(lttmp[1], ">")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ltIndex--;
        return num1 > num2;
    } else if (!strcmp(lttmp[1], "<")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ltIndex--;
        return num1 < num2;
    } else if (!strcmp(lttmp[1], ">=")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ltIndex--;
        return num1 >= num2;
    } else if (!strcmp(lttmp[1], "<=")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ltIndex--;
        return num1 <= num2;
    } else if (!strcmp(lttmp[1], "=>")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ltIndex--;
        return num1 >= num2;
    } else if (!strcmp(lttmp[1], "=<")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ltIndex--;
        return num1 <= num2;
    }
    cerr = 1;
    ltreturn:;
    return -1;
}

char ltmp[2][CB_BUF_SIZE];

bool runlogic() {
    ltmp[0][0] = 0; ltmp[1][0] = 0;
    int32_t i = 0;
    while (cmd[i] == ' ') {i++;}
    int32_t j = i;
    while (cmd[j] != ' ' && cmd[j]) {j++;}
    copyStrSnip(cmd, i, j, ltmp[0]);
    upCase(ltmp[0]);
    cerr = 0;
    chkCmdPtr = ltmp[0];
    #include "logic.c"
    return false;
}

void initBaseMem() {
    getVal_tmp[0] = malloc(CB_BUF_SIZE);
    getVal_tmp[1] = malloc(CB_BUF_SIZE);
    getVal_tmp[2] = malloc(CB_BUF_SIZE);
    getVal_tmp[3] = malloc(CB_BUF_SIZE);
    getFunc_gftmp[0] = malloc(CB_BUF_SIZE);
    getFunc_gftmp[1] = malloc(CB_BUF_SIZE);
    bfnbuf = malloc(CB_BUF_SIZE);
}

#define nfree(ptr) free(ptr); ptr = NULL

void freeBaseMem() {
    nfree(getVal_tmp[0]);
    nfree(getVal_tmp[1]);
    nfree(getVal_tmp[2]);
    nfree(getVal_tmp[3]);
    nfree(getFunc_gftmp[0]);
    nfree(getFunc_gftmp[1]);
    nfree(bfnbuf);
}

void printError(int error) {
    getCurPos();
    if (curx != 1) printf("\n");
    if (inProg) {printf("Error %d on line %d of '%s': '%s': ", error, progLine, basefilename(progfnstr), cmd);}
    else {printf("Error %d: ", error);}
    switch (error) {
        default:
            fputs("Unknown", stdout);
            break;
        case 1:
            fputs("Syntax", stdout);
            break;
        case 2:
            fputs("Type mismatch", stdout);
            break;
        case 3:
            fputs("Argument count mismatch", stdout);
            break;
        case 4:
            printf("Invalid variable name or identifier: '%s'", errstr);
            break;
        case 5:
            fputs("Operation resulted in undefined", stdout);
            break;
        case 6:
            fputs("LOOP without DO", stdout);
            break;
        case 7:
            fputs("ENDIF without IF", stdout);
            break;
        case 8:
            fputs("ELSE without IF", stdout);
            break;
        case 9:
            fputs("NEXT without FOR", stdout);
            break;
        case 10:
            fputs("Expected expression", stdout);
            break;
        case 11:
            fputs("Unexpected ELSE", stdout);
            break;
        case 12:
            fputs("Reached DO limit", stdout);
            break;
        case 13:
            fputs("Reached IF limit", stdout);
            break;
        case 14:
            fputs("Reached FOR limit", stdout);
            break;
        case 15:
            printf("File not found: '%s'", errstr);
            break;
        case 16:
            fputs("Invalid data or data range exceeded", stdout);
            break;
        case 17:
            printf("Cannot change to directory '%s' (errno: [%d] %s)", errstr, errno, strerror(errno));
            break;
        case 18:
            fputs("Expected file instead of directory", stdout);
            break;
        case 19:
            fputs("Expected directory instead of file", stdout);
            break;
        case 20:
            fputs("File or directory error", stdout);
            break;
        case 21:
            printf("Permission error: '%s'", errstr);
            break;
        case 22:
            printf("Array index out of bounds: '%s'", errstr);
            break;
        case 23:
            printf("Variable is not an array: '%s'", errstr);
            break;
        case 24:
            printf("Variable is an array: '%s'", errstr);
            break;
        case 25:
            fputs("Array is already dimensioned", stdout);
            break;
        case 26:
            fputs("Memory error", stdout);
            break;
        case 27:
            printf("Failed to open file '%s' (errno: [%d] %s)", errstr, errno, strerror(errno));
            break;
        case 125:
            printf("Function only valid in program: '%s'", errstr);
            break;
        case 126:
            printf("Function not valid in program: '%s'", errstr);
            break;
        case 127:
            printf("Not a function: '%s'", errstr);
            break;
        case 253:
            printf("Command only valid in program: '%s'", arg[0]);
            break;
        case 254:
            printf("Command not valid in program: '%s'", arg[0]);
            break;
        case 255:
            printf("Not a command: '%s'", arg[0]);
            break;
        case -1:
            fputs("Internal error.", stdout);
            break;
    }
    putchar('\n');
}

void runcmd() {
    if (cmd[0] == 0) return;
    cerr = 0;
    bool lgc = runlogic();
    if (lgc) goto cmderr;
    if (dlstackp > -1) {if (dldcmd[dlstackp]) return;}
    if (itstackp > -1) {if (itdcmd[itstackp]) return;}
    if (fnstackp > -1) {if (fndcmd[fnstackp]) return;}
    mkargs();
    argt = (uint8_t*)realloc(argt, argct + 1);
    arg = (char**)realloc(arg, (argct + 1) * sizeof(char*));
    for (int i = 1; i <= argct; ++i) {arg[i] = NULL;}
    solvearg(0);
    upCase(arg[0]);
    cerr = 255;
    chkCmdPtr = arg[0];
    #include "commands.c"
    cmderr:;
    if (cerr) {
        if (runc || runfile) err = 1;
        printError(cerr);
        cp = -1;
        concp = -1;
        chkinProg = inProg = false;
    }
    noerr:;
    if (lgc) return;
    for (int i = 0; i <= argct; ++i) {
        free(tmpargs[i]);
    }
    for (int i = 1; i <= argct; ++i) {
        free(arg[i]);
    }
    return;
}

