// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#define indy_uintptr_x indy_uintptr_64
#define indy_symbols_x indy_symbols_64
#define indy_thread_state_x x86_thread_state64_t
#define indy_thread_state_x_set_ip(s, v) { (s).__rip = (__uint64_t) (v); }
#define indy_thread_state_x_set_sp(s, v) { (s).__rsp = (__uint64_t) (v); }
#define INDY_TARGET_THREAD_X x86_THREAD_STATE64
#define INDY_TARGET_THREAD_X_COUNT x86_THREAD_STATE64_COUNT
#define indy_inject_x indy_inject_x86_64

#include "indy_x86_64_loader.inc.c"
#include "indy_x86.inc.c"
