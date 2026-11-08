/* bka_exit_hook.cpp — linker-wrap interceptors for exit/_exit/abort.
 * Android bionic does NOT provide <execinfo.h> / backtrace(); use dladdr +
 * __builtin_return_address(0) instead. Same result: names the immediate caller. */
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>

extern "C" void bka_diag_logfile(const char*);
extern "C" void __real_exit(int);
extern "C" void __real__exit(int);
extern "C" void __real_abort(void);

static void log_caller(const char* tag, int status_or_code) {
    char buf[512];
    void* caller = __builtin_extract_return_addr(__builtin_return_address(0));

    Dl_info info;
    const char* sym = "?";
    const char* dso = "?";
    unsigned long off = 0;
    if (caller && dladdr(caller, &info)) {
        if (info.dli_sname) { sym = info.dli_sname;
            off = (unsigned long)((char*)caller - (char*)info.dli_saddr); }
        if (info.dli_fname) dso = info.dli_fname;
    }
    snprintf(buf, sizeof(buf),
        "BKA-EXITTRACE %s code=%d caller=%p sym=%s+0x%lx dso=%s",
        tag, status_or_code, caller, sym, off, dso);
    bka_diag_logfile(buf);
}

extern "C" void __wrap_exit(int status) {
    log_caller("exit", status);
    __real_exit(status);
}
extern "C" void __wrap__exit(int status) {
    log_caller("_exit", status);
    __real__exit(status);
}
extern "C" void __wrap_abort(void) {
    log_caller("abort", 0);
    __real_abort();
}
