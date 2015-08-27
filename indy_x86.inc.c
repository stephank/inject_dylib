// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#include "indy_private.h"

// Defined in: libpthread/src/internal.h
#define _INTERNAL_POSIX_THREAD_KEYS_MAX 256

// Thread-specific data is just a bunch of pointers.
#define TSD_SIZE (_INTERNAL_POSIX_THREAD_KEYS_MAX * sizeof(indy_uintptr_x))

// Short-hand to align to target pointer size.
#define INDY_PTR_ALIGN __attribute__((aligned(sizeof(indy_uintptr_x))))

// Target version of indy_info.
struct indy_target_info {
    pid_t pid;

    INDY_PTR_ALIGN
    indy_uintptr_x dylib_path;
    indy_uintptr_x dylib_entry_symbol;

    indy_uintptr_x user_data;
    uint64_t user_data_size;

    indy_uintptr_x dylib_token;

    mach_vm_address_t region_addr;
    mach_vm_size_t region_size;

    mach_port_t port;

    INDY_PTR_ALIGN
    indy_uintptr_x thread;

    indy_uintptr_x dylib_handle;

    indy_uintptr_x tsd;

    indy_uintptr_x f_dlopen;
    indy_uintptr_x f_dlsym;
    indy_uintptr_x f_dlclose;

    indy_uintptr_x f_mach_task_self;
    indy_uintptr_x f_mach_port_deallocate;
    indy_uintptr_x f_mach_thread_self;
    indy_uintptr_x f_thread_terminate;

    indy_uintptr_x f_pthread_create;

    indy_uintptr_x f_sandbox_extension_consume;
}
__attribute__((packed));

bool indy_inject_x(struct indy_private *p, struct indy_error *err)
{
    kern_return_t kret;

    // Copy the program into the memory block.
    uint8_t *d_code = p->local_region;
    memcpy(d_code, program_code, sizeof(program_code));

    // Info structure is at the end of the code block
    struct indy_target_info *d_info =
    (void *) (d_code + sizeof(program_code) - sizeof(struct indy_target_info));

    // Symbols we need to match.
#define SYM(x) { .out = &d_info->f##x, .name = #x }
    struct indy_link_symbol dylib_symbols[] = {
        SYM(_dlopen),
        SYM(_dlsym),
        SYM(_dlclose)
    };
    struct indy_link_symbol kernel_symbols[] = {
        SYM(_mach_task_self),
        SYM(_mach_port_deallocate),
        SYM(_mach_thread_self),
        SYM(_thread_terminate)
    };
    struct indy_link_symbol pthread_symbols[] = {
        SYM(_pthread_create)
    };
    struct indy_link_symbol sandbox_symbols[] = {
        SYM(_sandbox_extension_consume)
    };
    struct indy_link match = {
        .num_images = 4,
        .images = (struct indy_link_image []) {
            { .name = "/usr/lib/system/libdyld.dylib",           .num_symbols = 3, .symbols = dylib_symbols   },
            { .name = "/usr/lib/system/libsystem_kernel.dylib",  .num_symbols = 4, .symbols = kernel_symbols  },
            { .name = "/usr/lib/system/libsystem_pthread.dylib", .num_symbols = 1, .symbols = pthread_symbols },
            { .name = "/usr/lib/system/libsystem_sandbox.dylib", .num_symbols = 1, .symbols = sandbox_symbols }
        }
    };
#undef SYM

    // Locate the symbols in the target address space.
    if (!indy_symbols_x(p->info->task, &match, err))
        return false;

    // Write library name, entry and sandbox extension.
    char *d_dylib_path = (void *) (d_info + 1);
    char *d_dylib_entry_symbol = stpcpy(d_dylib_path, p->info->dylib_path) + 1;
    char *d_dylib_token = stpcpy(d_dylib_entry_symbol, p->info->dylib_entry_symbol) + 1;
    uint8_t *d_user_data = (void *) (stpcpy(d_dylib_token, p->info->dylib_token) + 1);

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
    mach_vm_address_t base = p->info->region_addr - (mach_vm_address_t) p->local_region;
    d_info->pid                = p->info->pid;
    d_info->dylib_path         = (indy_uintptr_x) (base + (mach_vm_address_t) d_dylib_path);
    d_info->dylib_entry_symbol = (indy_uintptr_x) (base + (mach_vm_address_t) d_dylib_entry_symbol);
    d_info->user_data          = (indy_uintptr_x) (base + (mach_vm_address_t) d_user_data);
    d_info->user_data_size     = p->info->user_data_size;
    d_info->dylib_token        = (indy_uintptr_x) (base + (mach_vm_address_t) d_dylib_token);
    d_info->region_addr        = p->info->region_addr;
    d_info->region_size        = p->info->region_size;
    d_info->port               = p->target_port;
    d_info->tsd                = (indy_uintptr_x) (base + (mach_vm_address_t) d_tsd);

    // Setup the CPU state for the target thread.
    indy_thread_state_x s = { 0 };
    indy_thread_state_x_set_ip(s, base + (mach_vm_address_t) d_code);
    indy_thread_state_x_set_sp(s, base + (mach_vm_address_t) d_stack);

    // Start the thread.
    thread_act_t target_thread;
    kret = thread_create_running(p->info->task, INDY_TARGET_THREAD_X,
                                 (thread_state_t) &s, INDY_TARGET_THREAD_X_COUNT,
                                 &target_thread);
    return indy_set_error_ifneq(err, indy_couldnt_create_thread_in_target, kret, KERN_SUCCESS);
}
