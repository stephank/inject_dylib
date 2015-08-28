// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#define indy_uintptr_x indy_uintptr_32
#define indy_symbols_x indy_symbols_32
#define indy_thread_state_x x86_thread_state32_t
#define indy_thread_state_x_set_ip(s, v) { (s).__eip = (unsigned int) (v); }
#define indy_thread_state_x_set_sp(s, v) { (s).__esp = (unsigned int) (v); }
#define INDY_TARGET_THREAD_X x86_THREAD_STATE32
#define INDY_TARGET_THREAD_X_COUNT x86_THREAD_STATE32_COUNT
#define indy_setup_x indy_setup_i386

#include "indy_i386_loader.inc.c"
#include "indy_x86.inc.c"
