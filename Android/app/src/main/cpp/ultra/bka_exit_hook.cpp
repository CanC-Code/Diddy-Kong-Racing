/* bka_exit_hook.cpp — wrap the remaining exit paths that bypass exit()/_exit().
 * atexit catches the standard exit path (would prove a normal return).
 * _Exit / quick_exit bypass exit handlers — must be wrapped separately.
 * exit_group syscall is a last resort (would need seccomp/ptrace, skip). */
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>

#include <android/log.h>
extern "C" void bka_diag_logfile(const char*);
extern "C" void __real_exit(int);
extern "C" void __real__exit(int);
extern "C" void __real_abort(void);
extern "C" void __real__Exit(int);
extern "C" void __real_quick_exit(int);

static void log_caller(const char* tag, int code) {
    char buf[512];
    void* caller = __builtin_extract_return_addr(__builtin_return_address(0));
    Dl_info info = {0};
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
        tag, code, caller, sym, off, dso);
    __android_log_print(ANDROID_LOG_FATAL, "BKA-EXITLC", "%s", buf);
    bka_diag_logfile(buf);
}

static void atexit_handler(void) {
    bka_diag_logfile("BKA-EXITTRACE atexit fired (normal return from main)");
}

__attribute__((constructor))
static void install_atexit(void) {
    atexit(atexit_handler);
}

extern "C" void __wrap_exit(int status)      { log_caller("exit", status);       fflush(NULL); __real_exit(status); }
extern "C" void __wrap__exit(int status)     { log_caller("_exit", status);      fflush(NULL); __real__exit(status); }
extern "C" void __wrap_abort(void)           { log_caller("abort", 0);           fflush(NULL); __real_abort(); }
extern "C" void __wrap__Exit(int status)     { log_caller("_Exit", status);      fflush(NULL); __real__Exit(status); }
extern "C" void __wrap_quick_exit(int status){ log_caller("quick_exit", status); fflush(NULL); __real_quick_exit(status); }

extern "C" void __real_pthread_exit(void*);
extern "C" void __wrap_pthread_exit(void* retval) {
    log_caller("pthread_exit", 0);
    fflush(NULL);
    __real_pthread_exit(retval);
}

extern "C" void __real___stack_chk_fail(void);
extern "C" void __wrap___stack_chk_fail(void) {
    log_caller("stack_chk_fail", 0);
    fflush(NULL);
    __real___stack_chk_fail();
}
