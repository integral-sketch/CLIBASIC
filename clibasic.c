/* USER RE-DEFINABLE MACROS */

#ifndef CB_BUF_SIZE // Avoids redefinition error if '-DCB_BUF_SIZE=<number>' is used
    /* Sets the size of text buffers */
    #define CB_BUF_SIZE 32768 // Change the value to change the size of text buffers
#endif

#ifndef CB_PROG_LOGIC_MAX // Avoids redefinition error if '-DCB_PROG_LOGIC_MAX=<number>' is used
    /* Sets the size of logic command buffers */
    #define CB_PROG_LOGIC_MAX 256 // Change the value to change how far logic commands can be nested
#endif

#ifndef GCP_TIMEOUT // Avoids redefinition error if '-DGCP_TIMEOUT=<number>' is used
    /* Sets the timeout for getCurPos() before resending the escape code (slower terminals may require a higher value) */
    #define GCP_TIMEOUT 50000 // Change how long getCurPos() waits in microseconds until resending the cursor position request
#endif

/* Uses strcpy and strcat in place of copyStr and copyStrApnd */
#define BUILT_IN_STRING_FUNCS // Comment out this line to use CLIBASIC string functions

/* Uses strcmp in place of custom code */
#define BUILT_IN_COMMAND_COMPARE // Comment out this line to use the custom compare code when comparing commands and function strings

/* Sets what file CLIBASIC uses to store command history */
#define HIST_FILE ".clibasic_history" // Change the value to change where CLIBASIC puts the history file

/* Changes the terminal title to display the CLIBASIC version and bits */
#define CHANGE_TITLE // Comment out this line to disable changing the terminal/console title

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
    #include <dlfcn.h>
#else
    #include <windows.h>
    #include <conio.h>
    #include <fileapi.h>
#endif

#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

// Useful macros

/* Swap function to swap values */
#define swap(a, b) {__typeof__(a) c = a; a = b; b = c;}

/* Free & set to NULL combo */
#define nfree(ptr) {if (ptr) {free(ptr);} ptr = NULL;}

// Base defines

char VER[] = "0.28.1.4";

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

#if defined(FORCE_VT) && defined(_WIN_NO_VT)
    #undef _WIN_NO_VT
#endif

// Global vars and functions

#include "clibasic.h"

int progindex = -1;
char** progbuf = NULL;
char** progfn = NULL;
#define progfnstr (progfn[progindex])
int32_t* progcp = NULL;
int* progcmdl = NULL;
int* proglinebuf = NULL;

int err = 0;
int cerr;

bool inProg = false;
bool chkinProg = false;
int progLine = 1;

int varmaxct = 0;

cb_var* vardata = NULL;

char gpbuf[CB_BUF_SIZE];

char* cmd = NULL;
int cmdl = 0;
//char** tmpargs = NULL;
char** arg = NULL;
uint8_t* argt = NULL;
int32_t* argl = NULL;
int argct = -1;

int cmdpos = 0;

bool didloop = false;
bool lockpl = false;

typedef struct {
    uint8_t type;
    uint8_t block;
} cb_brkinfo;

cb_brkinfo brkinfo;
cb_brkinfo* oldbrkinfo = NULL;

typedef struct {
    int pl;
    int32_t cp;
    cb_brkinfo brkinfo;
} cb_jump;

cb_jump dlstack[CB_PROG_LOGIC_MAX];
bool dldcmd[CB_PROG_LOGIC_MAX];
int dlstackp = -1;
int* mindlstackp = NULL;

bool itdcmd[CB_PROG_LOGIC_MAX];
int itstackp = -1;
int* minitstackp = NULL;
bool didelse[CB_PROG_LOGIC_MAX];
bool didelseif[CB_PROG_LOGIC_MAX];

cb_jump fnstack[CB_PROG_LOGIC_MAX];
bool fndcmd[CB_PROG_LOGIC_MAX];
bool fninfor[CB_PROG_LOGIC_MAX];
int fnstackp = -1;
int* minfnstackp = NULL;
char fnvar[CB_BUF_SIZE];
char forbuf[4][CB_BUF_SIZE];

typedef struct {
    int pl;
    int32_t cp;
    int dlsp;
    int fnsp;
    int itsp;
    cb_brkinfo brkinfo;
} cb_gosub;

cb_gosub gsstack[CB_PROG_LOGIC_MAX];
int gsstackp = -1;

char* errstr = NULL;

char conbuf[CB_BUF_SIZE];
char prompt[CB_BUF_SIZE];
char pstr[CB_BUF_SIZE];
char cmpstr[CB_BUF_SIZE];

int curx = 0;
int cury = 0;

int concp = 0;
int32_t cp = 0;

bool cmdint = false;
bool inprompt = false;

bool runfile = false;
bool runc = false;
bool autorun = false;

bool redirection = false;
bool checknl = false;
bool esc = true;
bool cpos = true;
bool skip = false;

bool sh_silent = false;
bool sh_clearAttrib = true;
bool sh_restoreAttrib = true;

cb_txt txtattrib;

bool textlock = false;
bool sneaktextlock = false;

bool keep = false;

char* homepath = NULL;
char* cwd = NULL;

bool autohist = false;

int tab_width = 4;

int progargc = 0;
int* oldprogargc = NULL;
int newprogargc = 0;
char** progargs = NULL;
char*** oldprogargs = NULL;
char** newprogargs = NULL;
char* startcmd = NULL;
bool argslater = false;

bool changedtitle = false;
bool changedtitlecmd = false;

int retval = 0;

typedef struct {
    int pl;
    int32_t cp;
    bool used;
    char* name;
    int dlsp;
    int fnsp;
    int itsp;
    cb_brkinfo brkinfo;
} cb_goto;

cb_goto* gotodata = NULL;
cb_goto** proggotodata = NULL;
int gotomaxct = 0;
int* proggotomaxct = NULL;

cb_file* filedata = NULL;
int filemaxct = 0;
int fileerror = 0;

typedef struct {
    char* name;
    char* data;
} cb_sub;

cb_sub* subdata = NULL;
int submaxct = 0;
bool addsub = false;
int subindex = -1;

char* rl_tmpptr = NULL;

typedef struct {
    bool inuse;
    char* name;
    void* lib;
    int (*runcmd)(int, char**, uint8_t*, int32_t*);
    cb_funcret (*runfunc)(int, char**, uint8_t*, int32_t*, char*);
    int (*runlogic)(char*, char**, int32_t, int32_t);
    bool (*chkfuncsolve)(char*);
    void (*clearGlobals)(void);
    void (*promptReady)(void);
    bool (*deinit)(void);
} cb_ext;

int extmaxct = 0;
cb_ext* extdata = NULL;

#ifndef _WIN32
struct termios term, restore;
struct termios kbhterm, kbhterm2;
struct termios initterm;

void txtqunlock() {if (textlock || sneaktextlock) {tcsetattr(0, TCSANOW, &restore); textlock = false;}}

int kbhit() {
    int inchar;
    ioctl(0, FIONREAD, &inchar);
    return inchar;
}

sigset_t intmask, oldmask;

char inkeybuf[CB_BUF_SIZE];
#endif

static inline void* setsig(int sig, void* func) {
    #ifndef _WIN32
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

static inline void clearGlobals() {
    dlstackp = -1;
    itstackp = -1;
    fnstackp = -1;
    gsstackp = -1;
    memset(&brkinfo, 0, sizeof(brkinfo));
    for (int i = 0; i < CB_PROG_LOGIC_MAX; ++i) {
        memset(&dlstack[i], 0, sizeof(cb_jump));
        dldcmd[i] = false;
        memset(&fnstack[i], 0, sizeof(cb_jump));
        fnstack[i].cp = -1;
        fndcmd[i] = false;
        fninfor[i] = false;
        itdcmd[i] = false;
        didelse[i] = false;
        didelseif[i] = false;
        memset(&gsstack[i], 0, sizeof(cb_jump));
    }
    for (int i = 0; i < gotomaxct; ++i) {
        if (gotodata[i].used) nfree(gotodata[i].name);
    }
    nfree(gotodata);
    gotomaxct = 0;
    for (int i = extmaxct - 1; i > -1; --i) {
        if (extdata[i].inuse && extdata[i].clearGlobals) {
            extdata[i].clearGlobals();
        }
    }
}

static inline void promptReady() {
    for (int i = extmaxct - 1; i > -1; --i) {
        if (extdata[i].inuse && extdata[i].promptReady) {
            extdata[i].promptReady();
        }
    }
}

#ifdef _WIN32
#define hConsole GetStdHandle(STD_OUTPUT_HANDLE)
char* rlptr;
char kbinbuf[256];
bool inrl = false;
void cleanExit();
void cmdIntHndl();
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
#ifndef _WIN_NO_VT
bool vtenabled = false;
void enablevt() {
    if (vtenabled) return;
    vtenabled = true;
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
}
#endif
void updatechars() {
    char kbc;
    int i = strlen(kbinbuf) - 1;
    if (_kbhit()) {
        kbc = _getch();
        if (!((GetKeyState(VK_LCONTROL) | GetKeyState(VK_RCONTROL)) & 0x80)) {
            if (kbc == 13) kbc = 10;
            else if (kbc == 10) kbc = 13;
        }
        kbinbuf[++i] = kbc;
        if (!textlock) putchar(kbc);
        if (kbc == 3) {
            cmdIntHndl();
        }
    }
    kbinbuf[++i] = 0;
    fflush(stdout);
}
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 4
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(x) ((uint8_t)(x))
#define dlclose FreeLibrary
#define dlsym GetProcAddress
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
bool winArgNeedsQuotes(char* str) {
    for (int32_t i = 0; str[i]; ++i) {
        switch (str[i]) {
            case ' ':;
            case '&':;
            case '(':;
            case ')':;
            case '[':;
            case ']':;
            case '{':;
            case '}':;
            case '^':;
            case '=':;
            case ';':;
            case '!':;
            case '\'':;
            case '+':;
            case ',':;
            case '`':;
            case '~':;
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

void forceExit() {
    #ifndef _WIN32
    txtqunlock();
    tcsetattr(0, TCSANOW, &initterm);
    #endif
    putchar('\n');
    exit(0);
}

static inline void getCurPos();
static inline char* gethome();
static inline void initBaseMem();
static inline void freeBaseMem();
static inline void printError(int);
static inline void seterrstr(char*);
void unloadAllProg();
static inline char* basefilename(char*);
static inline char* pathfilename(char*);
int openFile(char*, char*);
bool closeFile(int);
static inline void upCase(char*);
uint8_t logictest(char*);
int loadExt(char*);
bool unloadExt(int);

void cleanExit() {
    txtqunlock();
    int ret;
    if (inprompt) {
        int i = kbhit();
        if (i) {ret = read(0, &gpbuf, i);}
        getCurPos();
        unloadAllProg();
        cmdint = true;
        putchar('\n');
        history_set_pos(history_length);
        rl_on_new_line();
        rl_replace_line("", 0);
        rl_pending_input = false;
        rl_redisplay();
        return;
    }
    setsig(SIGINT, forceExit);
    setsig(SIGTERM, forceExit);
    fflush(stdout);
    unloadAllProg();
    closeFile(-1);
    ret = chdir(gethome());
    (void)ret;
    if (autohist && !runfile) {
        write_history(HIST_FILE);
        #ifdef _WIN32
        SetFileAttributesA(HIST_FILE, FILE_ATTRIBUTE_HIDDEN);
        #endif
    }
    rl_clear_history();
    clear_history();
    #if defined(CHANGE_TITLE) && !defined(_WIN_NO_VT)
    if (esc && changedtitle) fputs("\e[23;0t", stdout);
    #endif
    #ifndef _WIN_NO_VT
    if (esc && !keep) fputs("\e[0m", stdout);
    #else
    if (!keep) SetConsoleTextAttribute(hConsole, ocAttrib);
    #endif
    #ifndef _WIN32
    int i = kbhit();
    while (i > 0) {getchar(); i--;}
    tcsetattr(0, TCSANOW, &kbhterm);
    #endif
    if (checknl) {
        getCurPos();
        if (curx != 1) putchar('\n');
    }
    freeBaseMem();
    for (int i = 0; i < varmaxct; ++i) {
        if (vardata[i].inuse) {
            if (vardata[i].size == -1) vardata[i].size = 0;
            for (int32_t j = 0; j <= vardata[i].size; ++j) {
                nfree(vardata[i].data[j]);
            }
            nfree(vardata[i].data);
            nfree(vardata[i].name);
        }
    }
    for (int i = 0; i < gotomaxct; ++i) {
        if (gotodata[i].used) {
            nfree(gotodata[i].name);
        }
    }
    for (int i = 0; i < argct; ++i) {
        nfree(arg[i]);
    }
    nfree(startcmd);
    nfree(rl_tmpptr);
    nfree(cmd);
    nfree(arg);
    nfree(argt);
    nfree(argl);
    nfree(errstr);
    nfree(vardata);
    if (progindex > -1) {
        nfree(progbuf[0]);
        nfree(progfn[0]);
    }
    nfree(progbuf);
    nfree(progfn);
    nfree(progcp);
    nfree(progcmdl);
    nfree(proglinebuf);
    nfree(minfnstackp);
    nfree(mindlstackp);
    nfree(minitstackp);
    nfree(proggotodata);
    nfree(proggotomaxct);
    nfree(oldprogargc);
    nfree(oldprogargs);
    clearGlobals();
    unloadExt(-1);
    #ifndef _WIN32
    tcsetattr(0, TCSANOW, &initterm);
    #endif
    exit(err);
}

void cmdIntHndl() {
    if (cmdint) setsig(SIGINT, cleanExit);
    cmdint = true;
}

void runcmd();
#ifdef BUILT_IN_STRING_FUNCS
    #define copyStr(b, a) strcpy(a, b)
    #define copyStrApnd(b, a) strcat(a, b)
#else
    static inline void copyStr(char*, char*);
    static inline void copyStrApnd(char*, char*);
#endif
static inline void copyStrApndNoEsc(char*, char*);
static inline void copyStrTo(char*, int32_t, char*);
static inline void copyStrSnip(char*, int32_t, int32_t, char*);
uint8_t getVal(char*, char*);
static inline void resetTimer();
bool loadProg(char*);
void unloadProg();
static inline void updateTxtAttrib();
static inline int isFile();
static inline uint64_t usTime();

static inline char* gethome() {
    if (!homepath) {
        #ifndef _WIN32
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

static inline char* basefilename(char* fn) {
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

static inline char* pathfilename(char* fn) {
    int32_t fnlen = strlen(fn);
    int32_t i;
    for (i = fnlen; i > -1; --i) {
        if (fn[i] == '/') break;
        #ifdef _WIN32
        if (fn[i] == '\\') break;
        #endif
    }
    copyStrTo(fn, i + 1, bfnbuf);
    return bfnbuf;
}

static inline void ttycheck() {
    if (redirection) return;
    if (!isatty(STDERR_FILENO)) {exit(1);}
    if (!isatty(STDIN_FILENO)) {fputs("CLIBASIC does not support STDIN redirection.\n", stderr); exit(1);}
    if (!isatty(STDOUT_FILENO)) {fputs("CLIBASIC does not support STDOUT redirection.\n", stderr); exit(1);}
}

static inline void readyTerm() {
    ttycheck();
    #ifndef _WIN32
    tcgetattr(0, &kbhterm);
    kbhterm2 = kbhterm;
    kbhterm2.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &kbhterm2);
    sigemptyset(&intmask);
    sigaddset(&intmask, SIGINT);
    #else
    #ifndef _WIN_NO_VT
    enablevt();
    #endif
    #endif
    if (!gethome()) {
        #ifndef _WIN32
        fputs("Could not find home folder! Please set the 'HOME' environment variable.\n", stderr);
        #else
        fputs("Could not find home folder!\n", stderr);
        #endif
    } else if (!skip) {
        char* tmpcwd = getcwd(NULL, 0);
        int ret = chdir(homepath);
        bool tmp_inProg = inProg;
        inProg = true;
        autorun = true;
        argslater = false;
        if (!loadProg(".clibasicrc"))
            if (!loadProg("autorun.bas"))
                if (!loadProg(".autorun.bas"))
                    {autorun = false; inProg = tmp_inProg;}
        ret = chdir(tmpcwd);
        nfree(tmpcwd);
        (void)ret;
    }
}

uint8_t roptptr = 1;
char roptstr[16] = "-";

#define RARG() {fprintf(stderr, "Short option '%c' requires argument(s) and must be last.\n", argv[i][shortopti]);}
#define IACT() {fputs("Incorrect number of arguments passed.\n", stderr);}
#define IOCT() {fputs("Incorrect number of options passed.\n", stderr);}

int main(int argc, char** argv) {
    bool pexit = false;
    bool info = false;
    #ifndef _WIN32
    tcgetattr(0, &initterm);
    #endif
    for (int i = 1; i < argc; ++i) {
        int shortopti = 0;
        bool shortopt;
        if (argv[i][0] == '-') {
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
                if (argc > 2) {IOCT(); exit(1);}
                printf("Command Line Interface BASIC version %s (%s %s-bit) using ", VER, OSVER, BVER);
                #ifdef _WIN32
                    fputs("Windows API calls", stdout);
                    #ifndef _WIN_NO_VT
                        fputs(" and VT escape codes", stdout);
                    #endif
                #else
                    fputs("VT escape codes", stdout);
                #endif
                putchar('\n');
                puts("Copyright (C) 2021 PQCraft");
                puts("This software is licensed under the GNU GPL v3");
                pexit = true;
            } else if (!strcmp(argv[i], "--help")) {
                if (argc > 2) {IOCT(); exit(1);}
                printf("Usage: %s [[-x] FILE [ARG]...]\n", argv[0]);
                puts("Options:");
                puts("    --help                      Displays the usage and option information.");
                puts("    --version                   Displays the version and license information.");
                puts("    -x, --exec FILE [ARG]...    Runs FILE and passes ARGs to FILE.");
                puts("    -c, --command COMMAND       Runs COMMAND as if in shell mode.");
                puts("    -k, --keep                  Stops CLIBASIC from resetting text attributes when exiting.");
                puts("    -s, --skip                  Skips searching for autorun programs.");
                puts("    -i, --info                  Displays an info string when starting in shell mode.");
                puts("    -r, --redirection           Allows for redirection (this may cause issues).");
                puts("    -n, --newline               Ensures the cursor is placed on a new line when exiting.");
                puts("    -e, --no-escapes            Disables escape codes.");
                puts("    -p, --no-curpos             Stops CLIBASIC from getting the cursor position.");
                pexit = true;
            } else if (!strcmp(argv[i], "--exec") || (shortopt && argv[i][shortopti] == 'x')) {
                if (shortopt && argv[i][shortopti + 1]) {RARG(); exit(1);}
                if (runc) {fputs("Cannot run command and file.\n", stderr); exit(1);}
                if (runfile) {unloadProg(); IOCT(); exit(1);}
                ++i;
                if (!argv[i]) {fputs("No filename provided.\n", stderr); exit(1);}
                argslater = true;
                if (!loadProg(argv[i])) {printError(cerr); exit(1);}
                inProg = true;
                runfile = true;
                progargs = (char**)malloc((argc - i) * sizeof(char*));
                for (progargc = 1; progargc < argc - i; ++progargc) {	
                    progargs[progargc] = malloc(strlen(argv[i + progargc]) + 1);
                    copyStr(argv[i + progargc], progargs[progargc]);
                }
                i = argc;
            } else if (!strcmp(argv[i], "--keep") || (shortopt && argv[i][shortopti] == 'k')) {
                if (keep) {IOCT(); exit(1);}
                keep = true;
                roptstr[roptptr++] = 'k';
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--skip") || (shortopt && argv[i][shortopti] == 's')) {
                if (skip) {IOCT(); exit(1);}
                skip = true;
                roptstr[roptptr++] = 's';
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--info") || (shortopt && argv[i][shortopti] == 'i')) {
                if (info) {IOCT(); exit(1);}
                info = true;
                roptstr[roptptr++] = 'i';
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--redirection") || (shortopt && argv[i][shortopti] == 'r')) {
                if (redirection) {IOCT(); exit(1);}
                redirection = true;
                roptstr[roptptr++] = 'r';
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--newline") || (shortopt && argv[i][shortopti] == 'n')) {
                if (checknl) {IOCT(); exit(1);}
                checknl = true;
                roptstr[roptptr++] = 'n';
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--no-escapes") || (shortopt && argv[i][shortopti] == 'e')) {
                if (!esc) {IOCT(); exit(1);}
                esc = false;
                roptstr[roptptr++] = 'e';
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--no-curpos") || (shortopt && argv[i][shortopti] == 'p')) {
                if (!cpos) {IOCT(); exit(1);}
                cpos = false;
                roptstr[roptptr++] = 'p';
                if (shortopt) goto chkshortopt;
            } else if (!strcmp(argv[i], "--command") || (shortopt && argv[i][shortopti] == 'c')) {
                if (shortopt && argv[i][shortopti + 1]) {RARG(); exit(1);}
                if (runfile) {fputs("Cannot run file and command.\n", stderr); exit(1);}
                if (runc) {IOCT(); exit(1);}
                i++;
                if (!argv[i]) {fputs( "No command provided.\n", stderr); exit(1);}
                runc = true;
                runfile = true;
                copyStr(argv[i], conbuf);
            } else {
                if (shortopt) {
                    fprintf(stderr, "Invalid short option '%c'.\n", argv[i][shortopti]); exit(1);
                } else {
                    fprintf(stderr, "Invalid long option '%s'.\n", argv[i]); exit(1);
                }
            }
        } else {
            if (runc) {fputs("Cannot run command and file.\n", stderr); exit(1);}
            if (runfile) {unloadProg(); IOCT(); exit(1);}
            if (!argv[i]) {fputs("No filename provided.\n", stderr); exit(1);}
            argslater = true;
            if (!loadProg(argv[i])) {printError(cerr); exit(1);}
            inProg = true;
            runfile = true;
            progargs = (char**)malloc((argc - i) * sizeof(char*));
            for (progargc = 1; progargc < argc - i; ++progargc) {	
                progargs[progargc] = malloc(strlen(argv[i + progargc]) + 1);
                copyStr(argv[i + progargc], progargs[progargc]);
            }
            i = argc;
        }
    }
    if (pexit) exit(0);
    roptstr[roptptr++] = 'x';
    readyTerm();
    rl_readline_name = "CLIBASIC";
    rl_tmpptr = calloc(1, 1);
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
    enablevt();
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
                startcmd[scsize] = 0;
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
                nfree(tmpstartcmd);
                startcmd = realloc(startcmd, strlen(startcmd) + 1);
            #endif
        #else
            uint32_t tmpsize = CB_BUF_SIZE;
            if (_NSGetExecutablePath(startcmd, &tmpsize)) {
                goto scargv;
            }
            char* tmpstartcmd = realpath(startcmd, NULL);
            swap(startcmd, tmpstartcmd);
            nfree(tmpstartcmd);
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
        nfree(startcmd);
        #ifndef _WIN32
        startcmd = realpath(argv[0], NULL);
        #else
        startcmd = _fullpath(NULL, argv[0], CB_BUF_SIZE);
        #endif
    } else {
        startcmd = argv[0];
    }
    skipscargv:;
    txtattrib.fgce = true;
    txtattrib.fgc = 15;
    txtattrib.truefgc = 0xFFFFFF;
    updateTxtAttrib();
    if (!runfile) {
        if (info) printf("Command Line Interface BASIC version %s (%s %s-bit)\n", VER, OSVER, BVER);
        strcpy(prompt, "\"CLIBASIC> \"");
        #ifdef CHANGE_TITLE
        #ifndef _WIN32
        if (esc) fputs("\e[22;0t", stdout);
        changedtitle = true;
        if (esc) printf("\e]2;CLIBASIC %s (%s-bit)%c", VER, BVER, 7);
        fflush(stdout);
        #else
        char* tmpstr = (char*)malloc(CB_BUF_SIZE);
        sprintf(tmpstr, "CLIBASIC %s (%s-bit)", VER, BVER);
        SetConsoleTitleA(tmpstr);
        nfree(tmpstr);
        #endif
        #endif
    }
    cmd = NULL;
    argt = NULL;
    arg = NULL;
    srand(usTime());
    if (!runfile && gethome()) {
        char* tmpcwd = getcwd(NULL, 0);
        int ret = chdir(homepath);
        FILE* tmpfile = fopen(HIST_FILE, "r");
        if ((autohist = (tmpfile != NULL))) {
            fclose(tmpfile);
            read_history(HIST_FILE);
            history_set_pos(history_length);
            HIST_ENTRY* tmphist = history_get(where_history());
            if (tmphist) copyStr(tmphist->line, cmpstr);
        }
        ret = chdir(tmpcwd);
        free(tmpcwd);
        (void)ret;
    }
    cerr = 0;
    initBaseMem();
    resetTimer();
    if (inProg || runc) {
        clearGlobals();
    }
    while (!pexit) {
        fchkint:;
        cp = 0;
        if (chkinProg) {inProg = true; chkinProg = false;}
        if (!inProg && !runc) {
            if (runfile) cleanExit();
            clearGlobals();
            promptReady();
            char* tmpstr = NULL;
            int tmpt = getVal(prompt, pstr);
            if (tmpt != 1) strcpy(pstr, "CLIBASIC> ");
            getCurPos();
            #ifndef _WIN32
            struct winsize max;
            ioctl(0, TIOCGWINSZ, &max);
            if (curx == max.ws_col) {
                putchar('\n');
            } else {
                curx--;
                int32_t ptr = strlen(pstr);
                while (curx > 0) {pstr[ptr] = 22; ptr++; curx--;}
                pstr[ptr] = 0;
            }
            #else
            if (curx > 1) putchar('\n');
            #endif
            updateTxtAttrib();
            inprompt = true;
            #ifdef _WIN32
            setsig(SIGINT, rl_sigh);
            #endif
            conbuf[0] = 0;
            #ifndef _WIN32
            setsig(SIGINT, cleanExit);
            #endif
            txtqunlock();
            tmpstr = readline(pstr);
            updateTxtAttrib();
            concp = 0;
            inprompt = false;
            if (!tmpstr) {err = 0; cleanExit();}
            int32_t tmpptr;
            if (tmpstr[0] == 0) {nfree(tmpstr); goto brkproccmd;}
            for (tmpptr = 0; tmpstr[tmpptr] == ' '; ++tmpptr) {}
            if (tmpptr) {
                int32_t iptr;
                for (iptr = 0; tmpstr[tmpptr]; ++tmpptr, ++iptr) {
                    tmpstr[iptr] = tmpstr[tmpptr];
                }
                tmpstr[iptr] = 0;
            }
            if (tmpstr[0] == 0) {nfree(tmpstr); goto brkproccmd;}
            tmpptr = strlen(tmpstr);
            if (tmpptr--) {
                while (tmpstr[tmpptr] == ' ' && tmpptr) {
                    tmpstr[tmpptr--] = 0;
                }
            }   
            if (strcmp(tmpstr, cmpstr)) {add_history(tmpstr); copyStr(tmpstr, cmpstr);}
            copyStr(tmpstr, conbuf);
            nfree(tmpstr);
            cmdint = false;
        }
        if (runc) runc = false;
        cmdl = 0;
        didloop = false;
        bool inStr = false;
        if (!runfile) setsig(SIGINT, cmdIntHndl);
        progLine = 1;
        bool comment = false;
        while (1) {
            rechk:;
            if (progindex < 0) {inProg = false;}
            else if (inProg == false) {progindex = - 1;}
            if (inProg) {
                if (progbuf[progindex][cp] == '"') {inStr = !inStr; cmdl++;} else
                if ((progbuf[progindex][cp] == ':' && !inStr) || progbuf[progindex][cp] == '\n' || progbuf[progindex][cp] == 0) {
                    if (cp - cmdl > 0 && progbuf[progindex][cp - cmdl - 1] == '\n') {
                        if (!lockpl) progLine++;
                        if (inStr) inStr = false;
                    }
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
                        err = 0;
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
                if (!inStr) {conbuf[concp] = ((conbuf[concp] >= 'a' && conbuf[concp] <= 'z') ? conbuf[concp] - 32 : conbuf[concp]);}
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
        #ifndef _WIN32
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
    #ifndef _WIN32
    struct timespec dts;
    dts.tv_sec = d / 1000000;
    dts.tv_nsec = (d % 1000000) * 1000;
    nanosleep(&dts, NULL);
    #else
    uint64_t t = d + usTime();
    while (t > usTime() && !cmdint) {
        if (!(usTime() % 5000))updatechars();
    }
    #endif
}

static inline int isFile(char* path) {
    struct stat pathstat;
    if (stat(path, &pathstat)) return -1;
    return !(S_ISDIR(pathstat.st_mode));
}

#ifndef _WIN32
bool gcp_sig = true;
#endif

static inline void getCurPos() {
    fflush(stdout);
    cury = 0; curx = 0;
    #ifndef _WIN32
    if (!cpos || !esc) {curx = 1; cury = 0; return;}
    if (gcp_sig) sigprocmask(SIG_SETMASK, &intmask, &oldmask);
    static char buf[16];
    for (int i = 0; i < 16; ++i) {buf[i] = 0;}
    register int i = 0;
    if (!textlock) {
        sneaktextlock = true;
        tcgetattr(0, &term);
        tcgetattr(0, &restore);
        term.c_lflag &= ~(ICANON|ECHO);
        tcsetattr(0, TCSANOW, &term);
    }
    i = kbhit();
    int32_t ikbp = strlen(inkeybuf);
    while (i > 0) {inkeybuf[ikbp++] = getchar(); --i;}
    inkeybuf[ikbp] = 0;
    for (int r = 0; r < 2; ++r) {
        i = 0;
        resend:;
        fputs("\e[6n", stdout);
        fflush(stdout);
        uint64_t tmpus = usTime();
        while (!kbhit()) {if (usTime() - tmpus > GCP_TIMEOUT) {goto resend;}}
        while (kbhit()) {
            if (kbhit()) {
                int ret = read(0, &buf[i], 1);
                (void)ret;
                if (buf[i] == 'R' || buf[i] == '\n' || kbhit() < 1) {++i; goto gcplexit;}
                ++i;
            }
        }
        gcplexit:;
        buf[i] = 0;
        i = kbhit();
        while (i > 0) {getchar(); --i;}
    }
    if (!textlock) {
        tcsetattr(0, TCSANOW, &restore);
        sneaktextlock = false;
    }
    i = kbhit();
    while (i > 0) {getchar(); --i;}
    if (buf[0] == '\e') {
        sscanf(buf, "\e[%d;%dR", &cury, &curx);
    } else {
        sscanf(buf, "[%d;%dR", &cury, &curx);
    }
    if (curx == 0 || cury == 0) {gcp_sig = false; getCurPos(); gcp_sig = true;}
    if (gcp_sig) sigprocmask(SIG_SETMASK, &oldmask, NULL);
    #else
    CONSOLE_SCREEN_BUFFER_INFO con;
    GetConsoleScreenBufferInfo(hConsole, &con);
    curx = con.dwCursorPosition.X + 1;
    cury = con.dwCursorPosition.Y + 1;
    #endif
}

void unloadProg() {
    for (int i = 1; i < progargc; ++i) {
        nfree(progargs[i]);
    }
    nfree(progargs);
    progargs = oldprogargs[progindex];
    progargc = oldprogargc[progindex];
    nfree(progbuf[progindex]);
    nfree(progfn[progindex]);
    progfn = (char**)realloc(progfn, progindex * sizeof(char*));
    progbuf = (char**)realloc(progbuf, progindex * sizeof(char*));
    for (int i = 0; i < gotomaxct; ++i) {
        if (gotodata[i].used) nfree(gotodata[i].name);
    }
    nfree(gotodata);
    gotodata = proggotodata[progindex];
    gotomaxct = proggotomaxct[progindex];
    cp = progcp[progindex];
    cmdl = progcmdl[progindex];
    progLine = proglinebuf[progindex];
    brkinfo = oldbrkinfo[progindex];
    dlstackp = mindlstackp[progindex];
    itstackp = minitstackp[progindex];
    fnstackp = minfnstackp[progindex];
    progcp = (int32_t*)realloc(progcp, progindex * sizeof(int32_t));
    progcmdl = (int*)realloc(progcmdl, progindex * sizeof(int));
    proglinebuf = (int*)realloc(proglinebuf, progindex * sizeof(int));
    mindlstackp = (int*)realloc(mindlstackp, progindex * sizeof(int));
    minitstackp = (int*)realloc(minitstackp, progindex * sizeof(int));
    minfnstackp = (int*)realloc(minfnstackp, progindex * sizeof(int));
    oldbrkinfo = (cb_brkinfo*)realloc(oldbrkinfo, progindex * sizeof(cb_brkinfo));
    proggotodata = (cb_goto**)realloc(proggotodata, progindex * sizeof(cb_goto*));
    proggotomaxct = (int*)realloc(proggotomaxct, progindex * sizeof(int));
    progindex--;
    if (progindex < 0) inProg = false;
    if (autorun) autorun = false;
}

void unloadAllProg() {
    for (int i = 0; i <= progindex; ++i) {
        unloadProg();
    }
}

bool loadProg(char* filename) {
    #if defined(_WIN32) && !defined(_WIN_NO_VT)
    enablevt();
    #endif
    retval = 0;
    seterrstr(filename);
    cerr = 27;
    FILE* prog = fopen(filename, "r");
    if (!prog) {
        if (errno == ENOENT) cerr = 15;
        return false;
    }
    if (!isFile(filename)) {
        fclose(prog);
        cerr = 18;
        return false;
    }
    ++progindex;
    fseek(prog, 0, SEEK_END);
    progfn = (char**)realloc(progfn, (progindex + 1) * sizeof(char*));
    #ifdef _WIN32
    progfn[progindex] = _fullpath(NULL, filename, CB_BUF_SIZE);
    #else
    progfn[progindex] = realpath(filename, NULL);
    #endif
    ++progindex;
    progbuf = (char**)realloc(progbuf, progindex * sizeof(char*));
    progcp = (int32_t*)realloc(progcp, progindex * sizeof(int32_t));
    progcmdl = (int*)realloc(progcmdl, progindex * sizeof(int));
    proglinebuf = (int*)realloc(proglinebuf, progindex * sizeof(int));
    mindlstackp = (int*)realloc(mindlstackp, progindex * sizeof(int));
    minitstackp = (int*)realloc(minitstackp, progindex * sizeof(int));
    oldbrkinfo = (cb_brkinfo*)realloc(oldbrkinfo, progindex * sizeof(cb_brkinfo));
    minfnstackp = (int*)realloc(minfnstackp, progindex * sizeof(int));
    proggotodata = (cb_goto**)realloc(proggotodata, progindex * sizeof(cb_goto*));
    proggotomaxct = (int*)realloc(proggotomaxct, progindex * sizeof(int));
    oldprogargc = (int*)realloc(oldprogargc, progindex * sizeof(int));
    oldprogargs = (char***)realloc(oldprogargs, progindex * sizeof(char**));
    --progindex;
    progcp[progindex] = cp;
    progcmdl[progindex] = cmdl;
    proglinebuf[progindex] = progLine;
    mindlstackp[progindex] = dlstackp;
    minitstackp[progindex] = itstackp;
    oldbrkinfo[progindex] = brkinfo;
    minfnstackp[progindex] = fnstackp;
    proggotodata[progindex] = gotodata;
    proggotomaxct[progindex] = gotomaxct;
    oldprogargc[progindex] = progargc;
    oldprogargs[progindex] = progargs;
    gotodata = NULL;
    gotomaxct = 0;
    cp = 0;
    cmdl = 0;
    progLine = 1;
    memset(&brkinfo, 0, sizeof(brkinfo));
    if (argslater) {
        argslater = false;
    } else {
        progargc = newprogargc;
        newprogargc = 0;
        progargs = newprogargs;
        newprogargs = NULL;
    }
    int32_t fsize = (uint32_t)ftell(prog);
    fseek(prog, 0, SEEK_SET);
    progbuf[progindex] = (char*)malloc(fsize + 1);
    int32_t j = 0;
    bool comment = false;
    bool inStr = false;
    bool sawCmd = false;
    while (j < fsize && !feof(prog)) {
        int tmpc = fgetc(prog);
        if (tmpc == '"') inStr = !inStr;
        if (!inStr && !sawCmd && tmpc == ' ') {sawCmd = true; inStr = false;}
        if (!inStr && (tmpc == '\'' || tmpc == '#')) comment = true;
        if (tmpc == '\n') {comment = false; inStr = false;}
        if (tmpc == '\r' || tmpc == '\t') tmpc = ' ';
        if (tmpc < 0) tmpc = 0;
        if (!comment) {progbuf[progindex][j] = (char)((inStr) ? tmpc : ((tmpc >= 'a' && tmpc <= 'z') ? tmpc -= 32 : tmpc)); j++;}
    }
    progbuf[progindex][j] = 0;
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
        case '+':;
        case '-':;
        case '*':;
        case '/':;
        case '^':;
            return true;
            break;
        default:;
            return false;
            break;
    }
}

static inline bool isExSpChar(char c) {
    switch (c) {
        case '+':;
        case '-':;
        case '*':;
        case '/':;
        case '^':;
        case '=':;
        case '<':;
        case '>':;
        case ',':;
            return true;
            break;
        default:;
            return false;
            break;
    }
}

static inline bool isValidVarChar(char c) {
    switch (c) {
        case 'A' ... 'Z':;
        case '0' ... '9':;
        case '#':;
        case '$':;
        case '%':;
        case '!':;
        case '?':;
        case '@':;
        case '[':;
        case ']':;
        case '_':;
        case '~':;
        case '.':;
            return true;
            break;
        default:;
            return false;
            break;
    }
}

static inline bool isValidHexChar(char c) {
    switch (c) {
        case 'a' ... 'f':;
        case 'A' ... 'F':;
        case '0' ... '9':;
            return true;
            break;
        default:;
            return false;
            break;
    }
}
#pragma GCC diagnostic pop

#ifndef BUILT_IN_STRING_FUNCS
static inline void copyStr(char* str1, char* str2) {
    for (; *str1; ++str1, ++str2) {*str2 = *str1;}
    *str2 = 0;
}

static inline void copyStrApnd(char* str1, char* str2) {
    while (*str2) {++str2;}
    for (; *str1; ++str1, ++str2) {*str2 = *str1;}
    *str2 = 0;
}
#endif

static inline void copyStrApndQ(char* str1, char* str2) {
    while (*str2) {++str2;}
    *str2++ = '"';
    for (; *str1; ++str1, ++str2) {*str2 = *str1;}
    *str2++ = '"';
    *str2 = 0;
}
static inline void copyStrApndNoEsc(char* str1, char* str2) {
    while (*str2) {++str2;}
    for (; *str1; ++str1, ++str2) {
        if ((*str2 = *str1) == '\\') {*str2 = '\\';}
        if (*str1 == '\'') {*str2 = '\\'; *++str2 = '\'';}
    }
    *str2 = 0;
}

static inline void copyStrSnip(char* str1, int32_t i, int32_t j, char* str2) {
    str1 += i;
    for (; i < j && *str1; ++str1, ++str2, ++i) {*str2 = *str1;}
    *str2 = 0;
}

static inline void copyStrTo(char* str1, int32_t i, char* str2) {
    int32_t i2 = 0;
    for (; *str1 && i2 < i; ++str1, ++str2, ++i2) {*str2 = *str1;}
    *str2 = 0;
}

static inline void copyStrFrom(char* str1, int32_t i, char* str2) {
    str1 += i;
    for (; *str1; ++str1, ++str2) {*str2 = *str1;}
    *str2 = 0;
}

static inline void strApndChar(char* str, char c) {
    while (*str) {++str;}
    *str = c;
    ++str;
    *str = 0;
}

static inline void upCase(char* str) {
    for (;*str; ++str) {
        if (*str >= 'a' && *str <= 'z') *str -= 32;
    }
}

static inline void lowCase(char* str) {
    for (;*str; ++str) {
        if (*str >= 'A' && *str <= 'Z') *str += 32;
    }
}

static inline void seterrstr(char* newstr) {
    size_t nslen = strlen(newstr);
    errstr = (char*)realloc(errstr, nslen + 1);
    copyStr(newstr, errstr);
}

static inline void updateTxtAttrib() {
    #ifndef _WIN_NO_VT
    if (!esc) return;
    fputs("\e[0m", stdout);
    if (txtattrib.fgce) {
        if (txtattrib.truecolor) printf("\e[38;2;%u;%u;%um", (uint8_t)(txtattrib.truefgc >> 16), (uint8_t)(txtattrib.truefgc >> 8), (uint8_t)txtattrib.truefgc);
        else printf("\e[38;5;%um", txtattrib.fgc);
    }
    if (txtattrib.bgce) {
        if (txtattrib.truecolor) printf("\e[48;2;%u;%u;%um", (uint8_t)(txtattrib.truebgc >> 16), (uint8_t)(txtattrib.truebgc >> 8), (uint8_t)txtattrib.truebgc);
        else printf("\e[48;5;%um", txtattrib.bgc);
    }
    if (txtattrib.bold) fputs("\e[1m", stdout);
    if (txtattrib.italic) fputs("\e[3m", stdout);
    if (txtattrib.underln) fputs("\e[4m", stdout);
    if (txtattrib.underlndbl) fputs("\e[21m", stdout);
    if (txtattrib.underlnsqg) fputs("\e[4:3m", stdout);
    if (txtattrib.strike) fputs("\e[9m", stdout);
    if (txtattrib.overln) fputs("\e[53m", stdout);
    if (txtattrib.dim) fputs("\e[2m", stdout);
    if (txtattrib.blink) fputs("\e[5m", stdout);
    if (txtattrib.hidden) fputs("\e[8m", stdout);
    if (txtattrib.reverse) fputs("\e[7m", stdout);
    if (txtattrib.underlncolor) printf("\e[58:5:%um", txtattrib.underlncolor);
    #else
    uint8_t tmpfgc, tmpbgc;
    if (txtattrib.truecolor) {
        uint8_t a;
        tmpfgc = (((txtattrib.truefgc >> 23) & 1) << 2) | (((txtattrib.truefgc >> 15) & 1) << 1) | ((txtattrib.truefgc >> 7) & 1);
        tmpfgc |= ((((txtattrib.truefgc >> 22) & 1) & ((txtattrib.truefgc >> 21) & 1)) << 2)\
            | ((((txtattrib.truefgc >> 14) & 1) & ((txtattrib.truefgc >> 13) & 1)) << 1)\
            | (((txtattrib.truefgc >> 6) & 1) & ((txtattrib.truefgc >> 5) & 1));
        a = ((((txtattrib.truefgc >> 16) & 0xFF) + ((txtattrib.truefgc >> 8) & 0xFF) + (txtattrib.truefgc & 0xFF)) / 3);
        tmpfgc |= (8 * (a > 84));
        tmpbgc = (((txtattrib.truebgc >> 23) & 1) << 2) | (((txtattrib.truebgc >> 15) & 1) << 1) | ((txtattrib.truebgc >> 7) & 1);
        tmpbgc |= ((((txtattrib.truebgc >> 22) & 1) & ((txtattrib.truebgc >> 21) & 1)) << 2)\
            | ((((txtattrib.truebgc >> 14) & 1) & ((txtattrib.truebgc >> 13) & 1)) << 1)\
            | (((txtattrib.truebgc >> 6) & 1) & ((txtattrib.truebgc >> 5) & 1));
        a = ((((txtattrib.truebgc >> 16) & 0xFF) + ((txtattrib.truebgc >> 8) & 0xFF) + (txtattrib.truebgc & 0xFF)) / 3);
        tmpbgc |= (8 * (a > 84));
    } else {
        uint8_t b1 = 0, b2 = 0;
        b1 = txtattrib.fgc & 1; b2 = (txtattrib.fgc >> 2) & 1; tmpfgc = (b1 ^ b2);
        tmpfgc = (tmpfgc) | (tmpfgc << 2); tmpfgc = txtattrib.fgc ^ tmpfgc;
        b1 = txtattrib.bgc & 1; b2 = (txtattrib.bgc >> 2) & 1; tmpbgc = (b1 ^ b2);
        tmpbgc = (tmpbgc) | (tmpbgc << 2); tmpbgc = txtattrib.bgc ^ tmpbgc;
    }
    if (txtattrib.dim) {tmpfgc %= 8; tmpbgc %= 8;}
    if (txtattrib.reverse) swap(tmpfgc, tmpbgc);
	SetConsoleTextAttribute(hConsole, (tmpfgc % 16) + ((tmpbgc % 16) << 4));
    #endif
    fflush(stdout);
}

char buf[CB_BUF_SIZE];

static inline void getStr(char* str1, char* str2) {
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
                case 'a': c = '\a'; break;
                case '[': c = 1; break;
                case ']': c = 2; break;
                case 'x':;
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

static inline uint8_t getType(char* str) {
    if (*str == '"') {if (str[strlen(str) - 1] != '"') {return 0;} return 1;}
    bool p = false;
    for (; *str; ++str) {
        if (*str == '-') {} else
        if ((*str < '0' || *str > '9') && *str != '.') {return 255;} else
        if (*str == '.') {if (p) {return 0;} p = true;}
    }
    return 2;
}

static inline bool isLineNumber(char* str) {
    if (!*str || *str == '-') return false;
    while (*str) {
        if (*str == '.') return false;
        if (*str < '0' || *str > '9') return false;
        ++str;
    }
    return true;
}

int cbrmIndex = 0;

bool cbrm(char* path) {
    fileerror = 0;
    if (isFile(path)) {
        if (remove(path)) {fileerror = errno; return false;}
        return true;
    }
    char* odir = (cbrmIndex) ? NULL : getcwd(NULL, 0);
    ++cbrmIndex;
    if (chdir(path)) {fileerror = errno; goto cbrm_fail;}
    DIR* cwd = opendir(".");
    struct dirent* dir;
    struct stat pathstat;
    while ((dir = readdir(cwd))) {
        if (strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")) {
            stat(dir->d_name, &pathstat);
            if (S_ISDIR(pathstat.st_mode)) {cbrm(dir->d_name);}
            else {remove(dir->d_name);}
        }
    }
    --cbrmIndex;
    int ret = chdir((cbrmIndex) ? ".." : odir);
    if (!cbrmIndex) nfree(odir);
    if (rmdir(path)) {fileerror = errno; return false;}
    return true;
    cbrm_fail:;
    --cbrmIndex;
    ret = chdir((cbrmIndex) ? ".." : odir);
    (void)ret;
    if (!cbrmIndex) nfree(odir);
    if (rmdir(path)) fileerror = errno;
    return false;
}

static inline int getArg(int, char*, char*);
static inline int getArgO(int, char*, char*, int32_t);
static inline int getArgCt(char*);

uint16_t getFuncIndex = 0;
char* getFunc_gftmp[2] = {NULL, NULL};

uint8_t getFunc(char* inbuf, char* outbuf) {
    char** farg;
    uint8_t* fargt;
    int32_t* flen;
    int fargct;
    int ftmpct = 0;
    int ftype = 0;
    char* gftmp[2];
    bool skipfargsolve = false;
    if (getFuncIndex) {
        gftmp[0] = malloc(CB_BUF_SIZE);
        gftmp[1] = malloc(CB_BUF_SIZE);
    } else {
        gftmp[0] = getFunc_gftmp[0];
        gftmp[1] = getFunc_gftmp[1];
    }
    ++getFuncIndex;
    int extsas = -1;
    {
        int32_t i;
        bool invalName = false;
        for (i = 0; inbuf[i] != '('; ++i) {if (!isValidVarChar(inbuf[i])) {invalName = true;}}
        if (invalName) {copyStrTo(inbuf, i, gpbuf); seterrstr(gpbuf); cerr = 4; return 0;}
        int32_t j = strlen(inbuf) - 1;
        copyStrSnip(inbuf, i + 1, j, gftmp[0]);
        fargct = getArgCt(gftmp[0]);
        farg = malloc((fargct + 1) * sizeof(char*));
        flen = malloc((fargct + 1) * sizeof(int32_t));
        fargt = malloc((fargct + 1) * sizeof(uint8_t));
        for (int j = 0; j <= fargct; ++j) {
            farg[j] = NULL;
        }
        int32_t gptr = 0;
        for (int j = 0; j <= fargct; ++j) {
            if (j == 0) {
                flen[0] = i;
                farg[0] = (char*)malloc(flen[0] + 1);
                copyStrTo(inbuf, i, farg[0]);
                for (int i = extmaxct - 1; i > -1; --i) {
                    if (extdata[i].inuse && extdata[i].chkfuncsolve) {
                        if ((skipfargsolve = extdata[i].chkfuncsolve(farg[0]))) {extsas = i; break;}
                    }
                }
                if (extsas == -1) {
                    if (!strcmp(farg[0], "~") || !strcmp(farg[0], "_TEST")) {
                        ftype = 2;
                        if (fargct != 1) {cerr = 3; goto fexit;}
                        cerr = 0;
                        if (getArgO(0, gftmp[0], gftmp[1], 0) == -1) {outbuf[0] = 0; goto fexit;}
                        int ret = logictest(gftmp[1]);
                        if (ret == -1) {outbuf[0] = 0; goto fexit;}
                        outbuf[0] = '0' + ret;
                        outbuf[1] = 0;
                        goto fexit;
                    } else if (!strcmp(farg[0], "EXECA") || !strcmp(farg[0], "EXECA$")) {
                        skipfargsolve = true;
                    }
                }
            } else {
                farg[j] = malloc(CB_BUF_SIZE);
                int32_t ngptr = getArgO(j - 1, gftmp[0], farg[j], gptr);
                if (ngptr == -1) {outbuf[0] = 0; goto fexit;}
                int32_t tmpsize = ngptr - gptr;
                gptr = ngptr;
                if (skipfargsolve) {
                    flen[j] = tmpsize;
                    farg[j] = realloc(farg[j], tmpsize + 1);
                } else {
                    fargt[j] = getVal(farg[j], farg[j]);
                    if (fargt[j] == 0) goto fnoerrscan;
                    if (fargt[j] == 255) fargt[j] = 0;
                    flen[j] = strlen(farg[j]);
                    farg[j] = realloc(farg[j], flen[j] + 1);
                }
                ftmpct++;
            }
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
        nfree(farg[j]);
    }
    nfree(farg);
    nfree(flen);
    nfree(fargt);
    --getFuncIndex;
    if (getFuncIndex) {nfree(gftmp[0]); nfree(gftmp[1]);}
    if (cerr) return 0;
    return ftype;
}

bool chkvar = true;

uint16_t getVarIndex = 0;
char* getVarBuf = NULL;

uint8_t getVar(char* vn, char* varout) {
    char* lgetVarBuf = (getVarIndex) ? malloc(CB_BUF_SIZE) : getVarBuf;
    ++getVarIndex;
    uint8_t ret = 0;
    int32_t vnlen = strlen(vn);
    if (vn[vnlen - 1] == ')') {
        ret = getFunc(vn, varout);
        goto gvret;
    }
    if (!vn[0] || vn[0] == '[' || vn[0] == ']') {
        cerr = 4;
        seterrstr(vn);
        goto gvret;
    }
    if (getType(vn) != 255) {
        cerr = 4;
        seterrstr(vn);
        goto gvret;
    }
    bool isArray = false;
    int32_t aindex = 0;
    for (register int32_t i = 0; vn[i]; ++i) {
        if (chkvar && !isValidVarChar(vn[i])) {
            cerr = 4;
            seterrstr(vn);
            goto gvret;
        }
        if (vn[i] == ']') {
            cerr = 1;
            goto gvret;
        }
        if (vn[i] == '[') {
            if (vn[vnlen - 1] != ']') {cerr = 1; goto gvret;}
            copyStrSnip(vn, i + 1, vnlen - 1, lgetVarBuf);
            if (!lgetVarBuf[0]) {cerr = 1; goto gvret;}
            cerr = 2;
            uint8_t tmpt = getVal(lgetVarBuf, lgetVarBuf);
            if (tmpt != 2) goto gvret;
            cerr = 0;
            aindex = atoi(lgetVarBuf);
            vn[i] = 0;
            vnlen = strlen(vn);
            isArray = true;
            break;
        }
    }
    int v = -1;
    for (register int i = 0; i < varmaxct; ++i) {
        if (vardata[i].inuse && !strcmp(vn, vardata[i].name)) {v = i; break;}
    }
    if (v == -1) {
        if (isArray) {
            cerr = 23;
            seterrstr(vn);
            goto gvret;
        }
        if (vn[vnlen - 1] == '$') {varout[0] = 0; ret = 1; goto gvret;}
        else {varout[0] = '0'; varout[1] = 0; ret = 2; goto gvret;}
    } else {
        if (vardata[v].size == -1) {
            if (isArray) {
                cerr = 23;
                seterrstr(vn);
                goto gvret;
            }
        } else {
            if (!isArray) {
                cerr = 24;
                seterrstr(vn);
                goto gvret;
            }
            if (aindex < 0 || aindex > vardata[v].size) {
                cerr = 22;
                sprintf(lgetVarBuf, "%s[%li]", vn, (long int)aindex);
                seterrstr(lgetVarBuf);
                goto gvret;
            }
        }
        copyStr(vardata[v].data[aindex], varout);
        ret = vardata[v].type;
        goto gvret;
    }
    gvret:;
    --getVarIndex;
    if (getVarIndex) free(lgetVarBuf);
    return ret;
}

char setVarBuf[CB_BUF_SIZE];

bool setVar(char* vn, char* val, uint8_t t, int32_t s) {
    int32_t vnlen = strlen(vn);
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
        if (chkvar && !isValidVarChar(vn[i])) {
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
            copyStrSnip(vn, i + 1, vnlen - 1, setVarBuf);
            if (!setVarBuf[0]) {cerr = 1; return 0;}
            cerr = 2;
            uint8_t tmpt = getVal(setVarBuf, setVarBuf);
            if (tmpt != 2) {return 0;}
            cerr = 0;
            aindex = atoi(setVarBuf);
            vn[i] = 0;
            vnlen = strlen(vn);
            isArray = true;
            break;
        }
    }
    int v = -1;
    for (register int i = 0; i < varmaxct; ++i) {
        if (vardata[i].inuse && !strcmp(vn, vardata[i].name)) {v = i; break;}
    }
    if (v == -1) {
        if (isArray) {
            cerr = 23;
            seterrstr(vn);
            return false;
        }
        for (register int i = 0; i < varmaxct; ++i) {
            if (!vardata[i].inuse) {v = i; break;}
        }
        if (v == -1) {
            v = varmaxct;
            varmaxct++;
            vardata = (cb_var*)realloc(vardata, varmaxct * sizeof(cb_var));
        }
        vardata[v].inuse = true;
        vardata[v].name = (char*)malloc(vnlen + 1);
        copyStr(vn, vardata[v].name);
        vardata[v].size = s;
        vardata[v].type = t;
        if (s == -1) s = 0;
        vardata[v].data = (char**)malloc((s + 1) * sizeof(char*));
        for (int32_t i = 0; i <= s; ++i) {
            vardata[v].data[i] = (char*)malloc(strlen(val) + 1);
            copyStr(val, vardata[v].data[i]);
        }
    } else {
        if (s != -1) {cerr = 25; return false;}
        if (t != vardata[v].type) {cerr = 2; return false;}
        if (isArray && (aindex < 0 || aindex > vardata[v].size)) {
            cerr = 22;
            sprintf(setVarBuf, "%s[%li]", vn, (long int)aindex);
            seterrstr(setVarBuf);
            return 0;
        }
        vardata[v].data[aindex] = (char*)realloc(vardata[v].data[aindex], strlen(val) + 1);
        copyStr(val, vardata[v].data[aindex]);
    }
    return true;
}

bool delVar(char* vn) {
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
        if (chkvar && !isValidVarChar(vn[i])) {
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
        if (vardata[i].inuse && !strcmp(vn, vardata[i].name)) {v = i; break;}
    }
    if (v != -1) {
        vardata[v].inuse = false;
        nfree(vardata[v].name);
        for (int32_t i = 0; i <= vardata[v].size; ++i) {
            nfree(vardata[v].data[i]);
        }
        nfree(vardata[v].data);
        if (v == varmaxct - 1) {
            while (!vardata[v].inuse && v >= 0) {varmaxct--; v--;}
            vardata = (cb_var*)realloc(vardata, varmaxct * sizeof(cb_var));
        }
    }
    return true;
}

int openFile(char* path, char* mode) {
    fileerror = 0;
    int i = 0;
    int j = -1;
    for (; i < filemaxct; ++i) {
        if (!filedata[i].fptr) {j = i; break;}
    }
    if (j == -1) {
        j = filemaxct;
        ++filemaxct;
        filedata = (cb_file*)realloc(filedata, filemaxct * sizeof(cb_file));
    }
    if (!(filedata[j].fptr = fopen(path, mode))) {
        fileerror = errno;
        --filemaxct;
        filedata = (cb_file*)realloc(filedata, filemaxct * sizeof(cb_file));
        return -1;
    }
    fseek(filedata[j].fptr, 0, SEEK_END);
    filedata[j].size = ftell(filedata[j].fptr);
    fseek(filedata[j].fptr, 0, SEEK_SET);
    return j;
}

bool closeFile(int num) {
    fileerror = 0;
    if (num > -1 && num < filemaxct) {
        if (filedata[num].fptr) {
            if (fclose(filedata[num].fptr)) {
                fileerror = errno;
                filedata[num].fptr = NULL;
                return false;
            }
            filedata[num].fptr = NULL;
            for (int i = filemaxct - 1; i > -1; --i) {
                if (!(filedata[i].fptr)) {
                    --filemaxct;
                } else {
                    break;
                }
            }
            filedata = (cb_file*)realloc(filedata, filemaxct * sizeof(cb_file));
        } else {
            return false;
        }
    } else {
        if (num == -1) {
            for (int i = 0; i < filemaxct; ++i) {
                if (filedata[i].fptr) {
                    fclose(filedata[i].fptr);
                    filedata[i].fptr = NULL;
                }
            }
            filemaxct = 0;
        } else {
            fileerror = EINVAL;
            return false;
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
        if (i > 0 && isSpChar(tmp[i - 1])) {
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
    tmp[0][0] = 0; tmp[1][0] = 0; tmp[2][0] = 0; tmp[3][0] = 0;
    seenStr = malloc(sizeof(bool));
    seenStr[0] = false;
    for (register int32_t i = 0; inbuf[i]; ++i) {
        switch (inbuf[i]) {
            default:;
                if (inStr) break;
                if (inbuf[i] == ',' || isSpChar(inbuf[i])) seenStr[pct] = false;
                break;
            case '&':;
                if (!inStr && pct > 0) seenStr[pct] = false;
                break;
            case '|':;
                if (!inStr && pct > 0) seenStr[pct] = false;
                break;
            case '"':;
                inStr = !inStr;
                if (inStr && seenStr[pct]) {
                    dt = 0;
                    cerr = 1;
                    goto gvreturn;
                }
                seenStr[pct] = true;
                break;
            case '(':;
                if (inStr) break;
                if (pct == 0) {ip = i;}
                pct++;
                seenStr = (bool*)realloc(seenStr, (pct + 1) * sizeof(bool));
                seenStr[pct] = false;
                break;
            case ')':;
                if (inStr) break;
                pct--;
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
                    copyStrFrom(inbuf, jp + 1, tmp[1]);
                    inbuf[ip] = 0;
                    if (t == 1) copyStrApndQ(tmp[0], inbuf);
                    else copyStrApnd(tmp[0], inbuf);
                    copyStrApnd(tmp[1], inbuf);
                    tmp[0][0] = 0; tmp[1][0] = 0; tmp[2][0] = 0; tmp[3][0] = 0;
                    tmplen[1] = strlen(inbuf);
                    i -= tmplen[0] - tmplen[1];
                }
                break;
            case '[':;
                if (inStr) break;
                bct++;
                break;
            case ']':;
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
                    default:;
                        if ((inbuf[jp] != '-' || jp > 0) && isSpChar(inbuf[jp]) && !pct && !bct) goto gvwhileexit1;
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
                tmp[3][0] = 0;
                copyStrApndQ(tmp[0], tmp[3]);
                swap(tmp[0], tmp[3]);
            }
        }
        if (t && dt == 0) {dt = t;} else
        if ((t && t != dt)) {cerr = 2; dt = 0; goto gvreturn;} else
        if (t == 0) {cerr = 1; dt = 0; goto gvreturn;}
        if ((dt == 1 && inbuf[jp] != '+') && inbuf[jp]) {cerr = 1; dt = 0; goto gvreturn;}
        if (t == 1) {copyStrSnip(tmp[0], 1, strlen(tmp[0]) - 1, tmp[2]); copyStrApnd(tmp[2], tmp[1]);} else
        if (t == 2) {
            if (inbuf[jp - 1]) copyStrFrom(inbuf, jp, tmp[1]);
            else tmp[1][0] = 0;
            copyStrApnd(tmp[1], tmp[0]);
            register int32_t p1, p2, p3;
            bool inStr = false;
            pct = 0;
            bct = 0;
            while (1) {
                numAct = 0;
                p1 = 0, p2 = 0, p3 = 0;
                for (register int32_t i = 0; tmp[0][i]; ++i) {
                    switch (tmp[0][i]) {
                        case '"': if (!pct && !bct) inStr = !inStr; break;
                        case '(': pct++; break;
                        case ')': pct--; break;
                        case '[': bct++; break;
                        case ']': bct--; break;
                        case '^':;
                            if (!inStr && !pct && !bct) {
                                if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                p2 = i; numAct = 4;
                                if (p2) goto foundact;
                            }
                            break;
                    }
                }
                for (register int32_t i = 0; tmp[0][i]; ++i) {
                    switch (tmp[0][i]) {
                        case '"': if (!pct && !bct) inStr = !inStr; break;
                        case '(': pct++; break;
                        case ')': pct--; break;
                        case '[': bct++; break;
                        case ']': bct--; break;
                        case '*':;
                            if (!inStr && !pct && !bct) {
                                if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                p2 = i; numAct = 2;
                                if (p2) goto foundact;
                            }
                            break;
                        case '/':;
                            if (!inStr && !pct && !bct) {
                                if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                p2 = i; numAct = 3;
                                if (p2) goto foundact;
                            }
                            break;
                    }
                }
                for (register int32_t i = 0; tmp[0][i]; ++i) {
                    switch (tmp[0][i]) {
                        case '"': if (!pct && !bct) inStr = !inStr; break;
                        case '(': pct++; break;
                        case ')': pct--; break;
                        case '[': bct++; break;
                        case ']': bct--; break;
                        case '+':;
                            if (!inStr && !pct && !bct) {
                                if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                p2 = i; numAct = 0;
                                if (p2) goto foundact;
                            }
                            break;
                        case '-':;
                            if (!inStr && !pct && !bct) {
                                if (!gvchkchar(tmp[0], i)) {dt = 0; goto gvreturn;}
                                p2 = i; numAct = 1;
                                if (p2) goto foundact;
                            }
                            break;
                    }
                }
                foundact:;
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
                    if (!pct && !bct && tmp[0][i] == '"') inStr = !inStr;
                    if (!inStr) {
                        switch (tmp[0][i]) {
                            case '(': pct++; break;
                            case ')': pct--; break;
                            case '[': bct++; break;
                            case ']': bct--; break;
                            default:;
                                if (isSpChar(tmp[0][i]) && !inStr && !pct && !bct) {p1 = i; goto gvforexit1;}
                                break;
                        }
                    }
                }
                gvforexit1:;
                for (register int32_t i = p2 + 1; true; ++i) {
                    if (!pct && !bct && tmp[0][i] == '"') inStr = !inStr;
                    if (!inStr) {
                        switch (tmp[0][i]) {
                            case '(': pct++; break;
                            case ')': pct--; break;
                            case '[': bct++; break;
                            case ']': bct--; break;
                            case 0: p3 = i; goto gvforexit2;
                            default:;
                                if (isSpChar(tmp[0][i]) && i != p2 + 1 && !pct && !bct) {p3 = i; goto gvforexit2;}
                                break;
                        }
                    }
                }
                gvforexit2:;
                if (p1 != 0 && isSpChar(tmp[0][p1])) p1++;
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
                    case 4:;
                        if (num1 == 0) {if (num2 == 0) {cerr = 5; dt = 0; goto gvreturn;} num3 = 0; break;}
                        if (num2 == 0) {num3 = 1; break;}
                        num3 = pow(num1, num2);
                        break;
                }
                sprintf(tmp[1], "%lf", num3);
                int32_t i = 0, j = strlen(tmp[1]) - 1;
                bool dp = false;
                while (tmp[1][i]) {if (tmp[1][i++] == '.') {dp = true; tmp[1][i + 6] = 0; break;}}
                if (dp) {while (tmp[1][j] == '0') {--j;} if (tmp[1][j] == '.') {--j;}}
                i = (tmp[1][0] == '-'); dp = (bool)i;
                while (tmp[1][i] == '0') {++i;}
                if (!tmp[1][i] || tmp[1][i] == '.') {--i;}
                if (dp) tmp[1][--i] = '-';
                copyStrSnip(tmp[1], i, j + 1, tmp[2]);
                copyStrFrom(tmp[0], p3, tmp[3]);
                if (p1) copyStrTo(tmp[0], p1, tmp[1]);
                else {tmp[1][0] = 0;}
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
        if (!strcmp(tmp[1], ".")) {cerr = 1; dt = 0; goto gvreturn;}
        int32_t i = 0, j = strlen(tmp[1]) - 1;
        bool dp = false;
        while (tmp[1][i]) {if (tmp[1][i++] == '.') {dp = true; tmp[1][i + 6] = 0; break;}}
        if (dp) {while (tmp[1][j] == '0') {--j;} if (tmp[1][j] == '.') {--j;}}
        i = (tmp[1][0] == '-'); dp = (bool)i;
        while (tmp[1][i] == '0') {++i;}
        if (!tmp[1][i] || tmp[1][i] == '.') {--i;}
        if (dp) tmp[1][--i] = '-';
        if (i < 0) i = 0;
        copyStrSnip(tmp[1], i, j + 1, outbuf);
        if (outbuf[0] == '-' && outbuf[1] == '0' && outbuf[2] == 0) {outbuf[0] = '0'; outbuf[1] = 0;}
    } else {
        copyStr(tmp[1], outbuf);
    }
    if (outbuf[0] == 0 && dt != 1) {outbuf[0] = '0'; outbuf[1] = 0; dt = 2;}
    gvreturn:;
    getValIndex--;
    nfree(seenStr);
    if (getValIndex) {nfree(tmp[0]); nfree(tmp[1]); nfree(tmp[2]); nfree(tmp[3]);}
    return dt;
}

static inline bool solvearg(int i) {
    if (i == 0) {
        argt[0] = 0;
        argl[0] = strlen(arg[0]);
        return true;
    }
    argt[i] = 0;
    arg[i] = realloc(arg[i], CB_BUF_SIZE);
    argt[i] = getVal(arg[i], arg[i]);
    if (argt[i] == 0) return false;
    if (argt[i] == 255) {argt[i] = 0;}
    argl[i] = strlen(arg[i]);
    return true;
}

static inline int getArgCt(char* inbuf) {
    int ct = 0;
    bool inStr = false;
    int pct = 0, bct = 0;
    while (*inbuf == ' ') {++inbuf;}
    for (; *inbuf; ++inbuf) {
        if (ct == 0) ct = 1;
        if (*inbuf == '"') inStr = !inStr;
        if (!inStr) {
            switch (*inbuf) {
                case '(': ++pct; break;
                case ')': --pct; break;
                case '[': ++bct; break;
                case ']': --bct; break;
                case ',': if (pct == 0 && bct == 0) {++ct;}; break;
            }
        }
    }
    return ct;
}

static inline int getArg(int num, char* inbuf, char* outbuf) {
    bool inStr = false;
    bool lookingForSpChar = false;
    bool sawSpChar = false;
    int pct = 0, bct = 0;
    int ct = 0;
    int32_t len = 0;
    for (int32_t i = 0; inbuf[i] && ct <= num; ++i) {
        if (!inStr) {
            switch (inbuf[i]) {
                case '(': ++pct; break;
                case ')': --pct; break;
                case '[': ++bct; break;
                case ']': --bct; break;
            }
        }
        if (inbuf[i] == '"') inStr = !inStr;
        if (!inStr && pct == 0 && bct == 0 && inbuf[i] == ',') {++ct;}
        else if (ct == num) {
            if (!inStr) {
                if (inbuf[i] == ' ' && !sawSpChar && len > 0) {lookingForSpChar = true;}
                if (isExSpChar(inbuf[i])) {lookingForSpChar = false; sawSpChar = true;}
            }
            if (inStr || inbuf[i] != ' ') {
                if (!isExSpChar(inbuf[i])) sawSpChar = false;
                if (lookingForSpChar) {outbuf[0] = 0; cerr = 1; return -1;}
                outbuf[len] = inbuf[i];
                ++len;
            }
        }
    }
    outbuf[len] = 0;
    return len;
}

static inline int getArgO(int num, char* inbuf, char* outbuf, int32_t i) {
    bool inStr = false;
    bool lookingForSpChar = false;
    bool sawSpChar = false;
    int pct = 0, bct = 0;
    int ct = num;
    int32_t len = 0;
    for (; inbuf[i] && ct <= num; ++i) {
        if (!inStr) {
            switch (inbuf[i]) {
                case '(': ++pct; break;
                case ')': --pct; break;
                case '[': ++bct; break;
                case ']': --bct; break;
            }
        }
        if (inbuf[i] == '"') inStr = !inStr;
        if (!inStr && pct == 0 && bct == 0 && inbuf[i] == ',') {++ct;}
        else if (ct == num) {
            if (!inStr) {
                if (inbuf[i] == ' ' && !sawSpChar && len > 0) {lookingForSpChar = true;}
                if (isExSpChar(inbuf[i])) {lookingForSpChar = false; sawSpChar = true;}
            }
            if (inStr || inbuf[i] != ' ') {
                if (!isExSpChar(inbuf[i])) sawSpChar = false;
                if (lookingForSpChar) {outbuf[0] = 0; cerr = 1; return -1;}
                outbuf[len] = inbuf[i];
                ++len;
            }
        }
    }
    outbuf[len] = 0;
    return i;
}

char tmpbuf[2][CB_BUF_SIZE];

static inline void mkargs() {
    int32_t j = 0;
    while (cmd[j] == ' ') {++j;}
    int32_t h = j;
    bool sccmd = false;
    if (cmd[j] == '$' || cmd[j] == '@' || cmd[j] == '%') {
        int32_t tmpj = j + 1;
        while (cmd[tmpj] == ' ') {++tmpj;}
        if (cmd[tmpj] != '=') sccmd = true;
    }
    if (!sccmd) {
        while (cmd[h] != ' ' && cmd[h] != '=' && cmd[h]) {++h;}
    }
    copyStrFrom(cmd, (cmd[h]) ? h + 1 : h, tmpbuf[0]);
    if (!sccmd) {
        int32_t tmph = h;
        while (cmd[tmph] == ' ' && cmd[tmph]) {++tmph;}
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
    }
    for (int i = 0; i <= argct; ++i) {
        nfree(arg[i]);
    }
    argct = getArgCt(tmpbuf[0]);
    arg = (char**)realloc(arg, (argct + 1) * sizeof(char*));
    argt = (uint8_t*)realloc(argt, (argct + 1) * sizeof(uint8_t));
    argl = (int32_t*)realloc(argl, (argct + 1) * sizeof(int32_t));
    int32_t gptr = 0;
    copyStrSnip(cmd, j, ((sccmd) ? h + 1 : h), tmpbuf[0]);
    argl[0] = strlen(tmpbuf[0]);
    arg[0] = malloc(argl[0] + 1);
    copyStr(tmpbuf[0], arg[0]);
    copyStrFrom(cmd, (h >= argl[0]) ? argl[0] : h + 1, tmpbuf[0]);
    for (int i = 1; i <= argct; ++i) {
        int32_t ngptr = getArgO(i - 1, tmpbuf[0], tmpbuf[1], gptr);
        if (ngptr == -1) return;
        argl[i] = ngptr - gptr;
        gptr = ngptr;
        arg[i] = malloc(argl[i] + 1);
        copyStr(tmpbuf[1], arg[i]);
        arg[i][argl[i]] = 0;
    }
    if (argct == 1 && arg[1][0] == 0) {argct = 0;}
}

char* lttmp_tmp[3];
int logictestexpr_index = 0;

static inline uint8_t logictestexpr(char* inbuf) {
    int32_t tmpp = 0;
    uint8_t t1 = 0;
    uint8_t t2 = 0;
    int32_t p = 0;
    bool inStr = false;
    bool lookingForSpChar = false;
    bool sawSpChar = false;
    int pct = 0, bct = 0;
    int ret = 255;
    char* lttmp[3];
    if (!logictestexpr_index) {
        lttmp[0] = lttmp_tmp[0];
        lttmp[1] = lttmp_tmp[1];
        lttmp[2] = lttmp_tmp[2];
    } else {
        lttmp[0] = malloc(CB_BUF_SIZE);
        lttmp[1] = malloc(CB_BUF_SIZE);
        lttmp[2] = malloc(CB_BUF_SIZE);
    }
    ++logictestexpr_index;
    while (inbuf[p] == ' ') {++p;}
    if (!inbuf[p]) {cerr = 10; goto ltreturn;}
    bool ltskip = false;
    for (int32_t i = p; inbuf[i]; ++i) {
        if (!inStr) {
            switch (inbuf[i]) {
                case '(': ++pct; break;
                case ')': --pct; break;
                case '[': ++bct; break;
                case ']': --bct; break;
            }
        }
        if (inbuf[i] == '"') {inStr = !inStr;}
        if (inbuf[i + 1] == 0) {t2 = 255; copyStr("<>", lttmp[1]); ltskip = true;}
        if ((inbuf[i] == '<' || inbuf[i] == '=' || inbuf[i] == '>') && !inStr && pct == 0 && bct == 0) {p = i; break;}
        if (!inStr && pct == 0 && bct == 0) {
            if (inbuf[i] == ' ' && !sawSpChar) {lookingForSpChar = true;}
            if (isExSpChar(inbuf[i])) {lookingForSpChar = false; sawSpChar = true;}
        }
        if (inStr || inbuf[i] != ' ') {
            if (!isExSpChar(inbuf[i])) sawSpChar = false;
            if (lookingForSpChar) {cerr = 1; goto ltreturn;}
            lttmp[0][tmpp] = inbuf[i]; tmpp++;
        }
    }
    lttmp[0][tmpp] = 0;
    if (ltskip) goto ltskipget;
    tmpp = 0;
    for (int32_t i = p; true; ++i) {
        if (tmpp > 2) {cerr = 1; goto ltreturn;}
        if (inbuf[i] != '<' && inbuf[i] != '=' && inbuf[i] != '>') {p = i; break;} else
        {lttmp[1][tmpp] = inbuf[i]; tmpp++;}
    }
    lttmp[1][tmpp] = 0;
    tmpp = 0;
    inStr = false;
    lookingForSpChar = false;
    sawSpChar = false;
    pct = 0; bct = 0;
    while (inbuf[p] == ' ') {++p;}
    for (int32_t i = p; inbuf[i]; ++i) {
        if (!inStr) {
            switch (inbuf[i]) {
                case '(': ++pct; break;
                case ')': --pct; break;
                case '[': ++bct; break;
                case ']': --bct; break;
            }
        }
        if (inbuf[i] == '"') {inStr = !inStr;}
        if (inbuf[i] == 0) {cerr = 1; goto ltreturn;}
        if ((inbuf[i] == '<' || inbuf[i] == '=' || inbuf[i] == '>') && !inStr && pct == 0 && bct == 0) {p = i; break;}
        if (!inStr && pct == 0 && bct == 0) {
            if (inbuf[i] == ' ' && !sawSpChar) {lookingForSpChar = true;}
            if (isExSpChar(inbuf[i])) {lookingForSpChar = false; sawSpChar = true;}
        }
        if (inStr || inbuf[i] != ' ') {
            if (!isExSpChar(inbuf[i])) sawSpChar = false;
            if (lookingForSpChar) {cerr = 1; goto ltreturn;}
            lttmp[2][tmpp] = inbuf[i]; tmpp++;
        }
    }
    lttmp[2][tmpp] = 0;
    t2 = getVal(lttmp[2], lttmp[2]);
    if (t2 == 0) goto ltreturn;
    if (t2 == 255) {cerr = 1; goto ltreturn;}
    ltskipget:;
    t1 = getVal(lttmp[0], lttmp[0]);
    if (t1 == 0) goto ltreturn;
    if (t1 == 255) {cerr = 1; goto ltreturn;}
    if (t2 == 255) {
        t2 = t1;
        if (t2 != 1) copyStr("0", lttmp[2]);
    }
    if (t1 != t2) {cerr = 2; goto ltreturn;}
    if (!strcmp(lttmp[1], "=")) {
        ret = (uint8_t)(bool)!strcmp(lttmp[0], lttmp[2]);
        goto ltreturn;
    } else if (!strcmp(lttmp[1], "<>")) {
        ret = (uint8_t)(bool)strcmp(lttmp[0], lttmp[2]);
        goto ltreturn;
    } else if (!strcmp(lttmp[1], ">")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ret = num1 > num2;
        goto ltreturn;
    } else if (!strcmp(lttmp[1], "<")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ret = num1 < num2;
        goto ltreturn;
    } else if (!strcmp(lttmp[1], ">=")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ret = num1 >= num2;
        goto ltreturn;
    } else if (!strcmp(lttmp[1], "<=")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ret = num1 <= num2;
        goto ltreturn;
    } else if (!strcmp(lttmp[1], "=>")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ret = num1 >= num2;
        goto ltreturn;
    } else if (!strcmp(lttmp[1], "=<")) {
        if (t1 == 1) {cerr = 2; goto ltreturn;}
        double num1, num2;
        sscanf(lttmp[0], "%lf", &num1);
        sscanf(lttmp[2], "%lf", &num2);
        ret = num1 <= num2;
        goto ltreturn;
    }
    cerr = 1;
    ltreturn:;
    --logictestexpr_index;
    if (logictestexpr_index) {
        nfree(lttmp[0]);
        nfree(lttmp[1]);
        nfree(lttmp[2]);
    }
    return ret;
}

char* ltbuf_tmp = NULL;
int logictest_index = 0;

uint8_t logictest(char* inbuf) {
    bool inStr = false;
    int32_t i = 0, j = 0;
    int pct = 0, bct = 0;
    uint8_t ret = 0, out = 0;
    uint8_t logicActOld = 0;
    char* ltbuf;
    if (!logictest_index) {
        ltbuf = ltbuf_tmp;
    } else {
        ltbuf = malloc(CB_BUF_SIZE);
    }
    ++logictest_index;
    while (inbuf[i]) {
        uint8_t logicAct = 0;
        for (;inbuf[j] && !logicAct; ++j) {
            switch (inbuf[j]) {
                case '"': inStr = !inStr; break;
                case '(': if (!inStr) ++pct; break;
                case ')': if (!inStr) --pct; break;
                case '[': if (!inStr) ++bct; break;
                case ']': if (!inStr) --bct; break;
                case '|': if (!inStr && !pct && !bct) {logicAct = 1; --j;} break;
                case '&': if (!inStr && !pct && !bct) {logicAct = 2; --j;} break;
            }
        }
        copyStrSnip(inbuf, i, j, ltbuf);
        switch (logicActOld) {
            case 1:;
                if (out) goto ltexit;
                if ((ret = logictestexpr(ltbuf)) == 255) {out = 255; goto ltexit;}
                out |= ret;
                break;
            case 2:;
                if (!out) goto ltexit;
                if ((ret = logictestexpr(ltbuf)) == 255) {out = 255; goto ltexit;}
                out &= ret;
                break;
            default:;
                out = logictestexpr(ltbuf);
                break;
        }
        if (!inbuf[j]) break;
        i = ++j;
        if (!inbuf[i]) {if (inbuf[j - 1] == '|' || inbuf[j - 1] == '&') {cerr = 10; out = 255;} break;}
        logicActOld = logicAct;
    }
    ltexit:;
    --logictest_index;
    if (logictest_index) nfree(ltbuf);
    return out;
}

char ltmp[2][CB_BUF_SIZE];

bool runlogic() {
    ltmp[0][0] = 0; ltmp[1][0] = 0;
    int32_t i = 0;
    while (cmd[i] == ' ') {++i;}
    int32_t j = i;
    while (cmd[j] != ' ' && cmd[j]) {++j;}
    int32_t h = j;
    while (cmd[h] == ' ') {++h;}
    if (cmd[h] == '=') return false;
    copyStrSnip(cmd, i, j, ltmp[0]);
    if (isLineNumber(ltmp[0])) {
        int tmp = -1;
        for (int j = 0; j < gotomaxct; ++j) {
            if (!gotodata[j].used) {tmp = j; break;}
            else if (!strcmp(gotodata[j].name, ltmp[0])) {
                if (gotodata[j].cp == cmdpos) {goto skiplbl;}
                cerr = 28; return true;
            }
        }
        if (tmp == -1) {
            tmp = gotomaxct;
            ++gotomaxct;
            gotodata = realloc(gotodata, gotomaxct * sizeof(cb_goto));
        }
        gotodata[tmp].name = malloc(strlen(ltmp[0]) + 1);
        copyStr(ltmp[0], gotodata[tmp].name);
        gotodata[tmp].cp = cmdpos;
        gotodata[tmp].pl = progLine;
        gotodata[tmp].used = true;
        gotodata[tmp].dlsp = dlstackp;
        gotodata[tmp].fnsp = fnstackp;
        gotodata[tmp].itsp = itstackp;
        skiplbl:;
        while (cmd[i] != ' ' && cmd[i]) {++i;}
        while (cmd[i] == ' ') {++i;}
        j = i;
        while (cmd[j] != ' ' && cmd[j]) {++j;}
        h = j;
        while (cmd[h] == ' ') {++h;}
        copyStrSnip(cmd, i, j, ltmp[0]);
        copyStrFrom(cmd, i, cmd);
        j -= i;
        i = 0;
    }
    cerr = 0;
    chkCmdPtr = ltmp[0];
    #include "logic.c"
    return false;
}

static inline void initBaseMem() {
    getVal_tmp[0] = malloc(CB_BUF_SIZE);
    getVal_tmp[1] = malloc(CB_BUF_SIZE);
    getVal_tmp[2] = malloc(CB_BUF_SIZE);
    getVal_tmp[3] = malloc(CB_BUF_SIZE);
    getFunc_gftmp[0] = malloc(CB_BUF_SIZE);
    getFunc_gftmp[1] = malloc(CB_BUF_SIZE);
    bfnbuf = malloc(CB_BUF_SIZE);
    ltbuf_tmp = malloc(CB_BUF_SIZE);
    lttmp_tmp[0] = malloc(CB_BUF_SIZE);
    lttmp_tmp[1] = malloc(CB_BUF_SIZE);
    lttmp_tmp[2] = malloc(CB_BUF_SIZE);
    getVarBuf = malloc(CB_BUF_SIZE);
}

static inline void freeBaseMem() {
    nfree(getVal_tmp[0]);
    nfree(getVal_tmp[1]);
    nfree(getVal_tmp[2]);
    nfree(getVal_tmp[3]);
    nfree(getFunc_gftmp[0]);
    nfree(getFunc_gftmp[1]);
    nfree(bfnbuf);
    nfree(ltbuf_tmp);
    nfree(lttmp_tmp[0]);
    nfree(lttmp_tmp[1]);
    nfree(lttmp_tmp[2]);
    nfree(getVarBuf);
}

static inline void printError(int error) {
    getCurPos();
    if (curx != 1) putchar('\n');
    if (inProg) {printf("Error %d on line %d of '%s':\n%s\n", error, progLine, basefilename(progfnstr), cmd);}
    else {printf("Error %d: ", error);}
    switch (error) {
        default:;
            fputs("Unknown", stdout);
            break;
        case 1:;
            fputs("Syntax", stdout);
            break;
        case 2:;
            fputs("Type mismatch", stdout);
            break;
        case 3:;
            fputs("Argument count mismatch", stdout);
            break;
        case 4:;
            printf("Invalid variable name or identifier: '%s'", errstr);
            break;
        case 5:;
            fputs("Operation resulted in undefined", stdout);
            break;
        case 6:;
            fputs("LOOP without DO", stdout);
            break;
        case 7:;
            fputs("ENDIF without IF", stdout);
            break;
        case 8:;
            fputs("ELSE or ELSEIF without IF", stdout);
            break;
        case 9:;
            fputs("NEXT without FOR", stdout);
            break;
        case 10:;
            fputs("Expected expression", stdout);
            break;
        case 11:;
            fputs("Unexpected ELSE", stdout);
            break;
        case 12:;
            fputs("Reached DO limit", stdout);
            break;
        case 13:;
            fputs("Reached IF limit", stdout);
            break;
        case 14:;
            fputs("Reached FOR limit", stdout);
            break;
        case 15:;
            printf("File or directory not found: '%s'", errstr);
            break;
        case 16:;
            fputs("Invalid data or data range exceeded", stdout);
            break;
        case 17:;
            printf("Cannot change to directory '%s' (errno: [%d] %s)", errstr, errno, strerror(errno));
            break;
        case 18:;
            fputs("Expected file instead of directory", stdout);
            break;
        case 19:;
            fputs("Expected directory instead of file", stdout);
            break;
        case 20:;
            fputs("File or directory error", stdout);
            break;
        case 21:;
            printf("Permission error: '%s'", errstr);
            break;
        case 22:;
            printf("Array index out of bounds: '%s'", errstr);
            break;
        case 23:;
            printf("Variable is not an array: '%s'", errstr);
            break;
        case 24:;
            printf("Variable is an array: '%s'", errstr);
            break;
        case 25:;
            fputs("Array is already dimensioned", stdout);
            break;
        case 26:;
            fputs("Memory error", stdout);
            break;
        case 27:;
            printf("Failed to open file: '%s' (errno: [%d] %s)", errstr, errno, strerror(errno));
            break;
        case 28:;
            fputs("Label is already defined", stdout);
            break;
        case 29:;
            fputs("Label is not defined", stdout);
            break;
        case 30:;
            fputs("Not in DO/FOR block", stdout);
            break;
        case 31:;
            fputs("RETURN without GOSUB", stdout);
            break;
        case 32:;
            fputs("Reached GOSUB limit", stdout);
            break;
        case 33:;
            printf("Failed to open library: '%s'", errstr);
            #ifndef _WIN32
            printf(" (%s)", dlerror());
            #endif
            break;
        case 34:;
            printf("Not a CLIBASIC extension: '%s'", errstr);
            break;
        case 35:;
            printf("Failed to initialize extension: '%s'", errstr);
            break;
        case 36:;
            printf("Extension already loaded: '%s'", errstr);
            break;
        case 125:;
            printf("Function only valid in program: '%s'", errstr);
            break;
        case 126:;
            printf("Function not valid in program: '%s'", errstr);
            break;
        case 127:;
            printf("Not a function: '%s'", errstr);
            break;
        case 253:;
            printf("Command only valid in program: '%s'", arg[0]);
            break;
        case 254:;
            printf("Command not valid in program: '%s'", arg[0]);
            break;
        case 255:;
            printf("Not a command: '%s'", arg[0]);
            break;
    }
    putchar('\n');
}

int loadExt(char* path) {
    seterrstr(path);
    #ifndef _WIN32
    void* lib = dlopen(path, RTLD_LAZY);
    #else
    void* lib = LoadLibrary(path);
    #endif
    if (!lib) {cerr = 33; return -1;}
    char* oextname = (void*)dlsym(lib, "cbext_name");
    bool (*cbext_init)(cb_extargs) = (void*)dlsym(lib, "cbext_init");
    if (!oextname | !cbext_init) {cerr = 34; goto loadfail;}
    if (!oextname[0]) {cerr = 34; goto loadfail;}
    int e = -1;
    char* extname = (char*)malloc(strlen(oextname) + 1);
    copyStr(oextname, extname);
    upCase(extname);
    for (register int i = 0; i < extmaxct; ++i) {
        if (extdata[i].inuse && !strcmp(extname, extdata[i].name)) {
            seterrstr(extname);
            cerr = 36;
            goto loadfail;
        }
    }
    for (register int i = 0; i < extmaxct; ++i) {
        if (!extdata[i].inuse) {e = i; break;}
    }
    cb_extargs extargs = {
        VER, BVER, OSVER,
        &cerr, &retval, &fileerror,
        &varmaxct, vardata,
        &filemaxct, filedata,
        &chkCmdPtr,
        &txtattrib,
        &curx, &cury,
        startcmd, roptstr,
        getCurPos,
        gethome,
        seterrstr,
        basefilename, pathfilename,
        openFile, closeFile, cbrm,
        usTime, timer, resetTimer, cb_wait,
        updateTxtAttrib,
        randNum,
        chkCmd,
        isSpChar, isExSpChar, isValidVarChar, isValidHexChar,
        getArgCt, getArg, getArgO,
        getStr, getType,
        getVar, setVar, delVar,
        getVal,
        solvearg,
        logictest,
        printError
    };
    if (!cbext_init(extargs)) {cerr = 35; goto loadfail;}
    if (e == -1) {
        e = extmaxct;
        ++extmaxct;
        extdata = (cb_ext*)realloc(extdata, extmaxct * sizeof(cb_ext));
    }
    extdata[e].inuse = true;
    extdata[e].name = extname;
    extdata[e].lib = lib;
    extdata[e].runcmd = (void*)dlsym(lib, "cbext_runcmd");
    extdata[e].runfunc = (void*)dlsym(lib, "cbext_runfunc");
    extdata[e].runlogic = (void*)dlsym(lib, "cbext_runlogic");
    extdata[e].chkfuncsolve = (void*)dlsym(lib, "cbext_chkfuncsolve");
    extdata[e].clearGlobals = (void*)dlsym(lib, "cbext_clearGlobals");
    extdata[e].promptReady = (void*)dlsym(lib, "cbext_promptReady");
    extdata[e].deinit = (void*)dlsym(lib, "cbext_deinit");
    return e;
    loadfail:;
    dlclose(lib);
    return -1;
}

bool unloadExt(int e) {
    if (e == -1) {
        for (int i = extmaxct - 1; i > -1; --i) {
            if (!extdata[i].inuse) continue;
            if (extdata[i].deinit) extdata[i].deinit();
            extdata[i].inuse = false;
            nfree(extdata[i].name);
            dlclose(extdata[i].lib);
        }
        free(extdata);
        extmaxct = 0;
    } else {
        if (e < -1 || e >= extmaxct || !extdata[e].inuse) {cerr = 16; return false;}
        if (extdata[e].deinit) extdata[e].deinit();
        extdata[e].inuse = false;
        nfree(extdata[e].name);
        dlclose(extdata[e].lib);
        for (int i = extmaxct - 1; i > -1; --i) {
            if (extdata[i].inuse) break;
            --extmaxct;
        }
        extdata = (cb_ext*)realloc(extdata, extmaxct * sizeof(cb_ext));
     }
     return true;
}

void runcmd() {
    if (cmd[0] == 0) return;
    cerr = 0;
    bool lgc = runlogic();
    if (lgc) goto cmderr;
    if (cmd[0] == 0) return;
    int32_t tmpi = 0;
    while (cmd[tmpi] && cmd[tmpi] != ' ') {tmpi++;}
    if (!cmd[tmpi]) tmpi = -1;
    if (tmpi > -1) cmd[tmpi] = 0;
    chkCmdPtr = cmd;
    if (!chkCmd(2, "LABEL", "LBL") && cmd[0] != '@') {
        if (tmpi > -1) cmd[tmpi] = ' ';
        if (dlstackp > ((progindex > -1) ? mindlstackp[progindex] : -1)) {if (dldcmd[dlstackp]) return;}
        if (itstackp > ((progindex > -1) ? minitstackp[progindex] : -1)) {if (itdcmd[itstackp]) return;}
        if (fnstackp > ((progindex > -1) ? minfnstackp[progindex] : -1)) {if (fndcmd[fnstackp]) return;}
    }
    if (tmpi > -1) cmd[tmpi] = ' ';
    mkargs();
    if (cerr) goto cmderr;
    solvearg(0);
    cerr = 255;
    chkCmdPtr = arg[0];
    #include "commands.c"
    cmderr:;
    if (cerr) {
        err = 0;
        if (runc || runfile) err = 1;
        printError(cerr);
        cp = -1;
        concp = -1;
        chkinProg = inProg = false;
    }
    noerr:;
    if (lgc) return;
    for (int i = 0; i <= argct; ++i) {
        nfree(arg[i]);
    }
    argct = 0;
    return;
}

