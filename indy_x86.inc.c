// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#include "indy_private.h"

// Defined in: libpthread/src/internal.h
#define _INTERNAL_POSIX_THREAD_KEYS_END 768

// Thread-specific data is just a bunch of pointers.
#define TSD_SIZE (_INTERNAL_POSIX_THREAD_KEYS_END * sizeof(indy_uintptr_x))

// Short-hand to align to target pointer size.
#define INDY_PTR_ALIGN __attribute__((aligned(sizeof(indy_uintptr_x))))

// Target version of indy_info.
struct indy_info_x {
    pid_t pid;

    INDY_PTR_ALIGN
    indy_uintptr_x dylib_path;
    indy_uintptr_x dylib_entry_symbol;

    indy_uintptr_x user_data;
    uint64_t user_data_size;

    INDY_PTR_ALIGN
    indy_uintptr_x dylib_token;

    indy_uintptr_x tsd;

    uint32_t exit_status;

    INDY_PTR_ALIGN
    indy_uintptr_x f_dlopen;
    indy_uintptr_x f_dlsym;
    indy_uintptr_x f_dlclose;

    indy_uintptr_x f_mach_thread_self;
    indy_uintptr_x f_thread_terminate;

    indy_uintptr_x f_pthread_main_thread_np;
    indy_uintptr_x f_pthread_create;
    indy_uintptr_x f_pthread_join;

    indy_uintptr_x f_sandbox_extension_consume;
}
__attribute__((packed));

bool indy_setup_x(struct indy_private *p)
{
    kern_return_t kret;

    // Copy the program into the memory block.
    uint8_t *d_code = p->local_region;
    memcpy(d_code, program_code, sizeof(program_code));

    // Info structure is at the end of the code block
    struct indy_info_x *d_info =
        (void *) (d_code + sizeof(program_code) - sizeof(struct indy_info_x));

    // Symbols we need to match.
#define SYM(x) { .out = &d_info->f##x, .name = #x }
    struct indy_link_symbol dylib_symbols[] = {
        SYM(_dlopen),
        SYM(_dlsym),
        SYM(_dlclose)
    };
    struct indy_link_symbol kernel_symbols[] = {
        SYM(_mach_thread_self),
        SYM(_thread_terminate)
    };
    struct indy_link_symbol pthread_symbols[] = {
        SYM(_pthread_main_thread_np),
        SYM(_pthread_create),
        SYM(_pthread_join)
    };
    struct indy_link_symbol sandbox_symbols[] = {
        SYM(_sandbox_extension_consume)
    };
    struct indy_link match = {
        .num_images = 4,
        .images = (struct indy_link_image []) {
            { .name = "/usr/lib/system/libdyld.dylib",           .num_symbols = 3, .symbols = dylib_symbols   },
            { .name = "/usr/lib/system/libsystem_kernel.dylib",  .num_symbols = 2, .symbols = kernel_symbols  },
            { .name = "/usr/lib/system/libsystem_pthread.dylib", .num_symbols = 3, .symbols = pthread_symbols },
            { .name = "/usr/lib/system/libsystem_sandbox.dylib", .num_symbols = 1, .symbols = sandbox_symbols },
        }
    };
#undef SYM

    // Locate the symbols in the target address space.
    if (!indy_symbols_x(p, &match))
        return false;

    // Write library name, entry and sandbox extension.
    char *d_dylib_path = (void *) (d_info + 1);
    char *d_dylib_entry_symbol = stpcpy(d_dylib_path, p->info->dylib_path) + 1;
    char *d_dylib_token = stpcpy(d_dylib_entry_symbol, p->info->dylib_entry_symbol) + 1;
    uint8_t *d_user_data = (void *) (stpcpy(d_dylib_token, p->dylib_token) + 1);

    // Copy user data on 16-byte alignment.
    size_t user_data_size = p->info->user_data_size;
    while (((uintptr_t) d_user_data) & 15) d_user_data++;
    memcpy(d_user_data, p->info->user_data, user_data_size);

    // Locate thread-specific data on 16-byte alignment.
    uint8_t *d_tsd = d_user_data + user_data_size;
    while (((uintptr_t) d_tsd) & 15) d_tsd++;

    // Locate stack on 16-byte alignment.
    uint8_t *d_stack = d_tsd + TSD_SIZE;
    while (((uintptr_t) d_stack) & 15) d_stack++;

    // Fill the remaining data structure parts.
    mach_vm_address_t base = p->region_addr - (mach_vm_address_t) p->local_region;
    d_info->pid                = p->info->pid;
    d_info->dylib_path         = (indy_uintptr_x) (base + (mach_vm_address_t) d_dylib_path);
    d_info->dylib_entry_symbol = (indy_uintptr_x) (base + (mach_vm_address_t) d_dylib_entry_symbol);
    d_info->user_data          = (indy_uintptr_x) (base + (mach_vm_address_t) d_user_data);
    d_info->user_data_size     = p->info->user_data_size;
    d_info->dylib_token        = (indy_uintptr_x) (base + (mach_vm_address_t) d_dylib_token);
    d_info->tsd                = (indy_uintptr_x) (base + (mach_vm_address_t) d_tsd);
    d_info->exit_status        = 0xF00DFACE;
    p->exit_status_addr        = base + (mach_vm_address_t) &d_info->exit_status;

    // Setup the CPU state for the target thread.
    indy_thread_state_x s = { 0 };
    indy_thread_state_x_set_ip(s, base + (mach_vm_address_t) d_code);
    indy_thread_state_x_set_sp(s, base + (mach_vm_address_t) d_stack);
    kret = thread_set_state(p->target_thread, INDY_TARGET_THREAD_X, (thread_state_t) &s, INDY_TARGET_THREAD_X_COUNT);
    return indy_set_error_ifneq(p->res, indy_couldnt_set_target_thread_state, kret, KERN_SUCCESS);
}
