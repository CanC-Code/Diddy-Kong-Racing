#include <unistd.h>
#include <stdlib.h>
#include <execinfo.h>
#include <stdint.h>

extern "C" void bka_diag_logfile(const char*);
extern "C" void __real_exit(int);
extern "C" void __real__exit(int);
extern "C" void __real_abort(void);

static void dump_stack(const char* tag) {
    char buf[256];
    snprintf(buf, sizeof(buf), "BKA-EXITTRACE %s pid=%d", tag, (int)getpid());
    bka_diag_logfile(buf);

    void* frames[48];
    int n = backtrace(frames, 48);
    char** syms = backtrace_symbols(frames, n);
    for (int i = 0; i < n && i < 48; i++) {
        snprintf(buf, sizeof(buf), "BKA-EXITTRACE %s #%d %s", tag, i, syms[i]);
        bka_diag_logfile(buf);
    }
    free(syms);
}

extern "C" void __wrap_exit(int status) {
    dump_stack("exit");
    __real_exit(status);
}

extern "C" void __wrap__exit(int status) {
    dump_stack("_exit");
    __real__exit(status);
}

extern "C" void __wrap_abort(void) {
    dump_stack("abort");
    __real_abort();
}
