// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#ifndef indy_h
#define indy_h

#include <stdlib.h>
#include <stdbool.h>
#include <mach/mach.h>

// Info structure provided by the caller and completed by indy_inject.
// Zero this structure at initialization.
struct indy_info {

    //////////
    // Provided by the caller:

    // Target PID.
    pid_t pid;

    // Path and entry point of the dynamic library.
    const char *dylib_path;
    const char *dylib_entry_symbol;

    // User data to copy to the target process.
    const void *user_data;
    uint64_t user_data_size;

    //////////
    // Set on successful indy_inject:

    // Mach task port for the target. (Only on the injector.)
    mach_port_t task;

    // Sandbox extension token for reading the dynamic library.
    char *dylib_token;

    // Allocated memory region in the target process, containing the target
    // version of this structure, as well as a copy of the user data,
    // the loader code and stack.
    mach_vm_address_t region_addr;
    mach_vm_size_t region_size;

    // Mach port for communication. A receive right for the caller,
    // a send right for the target.
    mach_port_t port;

    // Handle to the created thread. (Only on the target.)
    pthread_t thread;

    // Handle to the dynamic library. (Only on the target.)
    void *dylib_handle;

};

// Structure containing error info.
//
// The errors generated may not be extremely useful to end-users. Ideally,
// common user errors would be documented here, so that the application may
// give useful feedback. For now, it's mostly useful in bug reports.
//
// The description can be compared to the string constants below, or printed as
// a format string along with os_ret: `sprintf(out, err.descr, err.os_ret);`
struct indy_error {
    const char *descr;
    int64_t os_ret;    // Usually a kern_return_t, but dependant on cause.
};

// Entry point signature.
typedef void (*indy_entry)(struct indy_info *info);

// Inject into a process. Returns success when all code and data was injected
// and the loader has started.
//
// Successful return does not mean loading the dynamic library has also
// succeeded. Use the Mach port to communicate and determine actual success.
//
// If the entry-point returns or could not be executed, the Mach port and
// library handle are both closed. Register for a no-sender notification on the
// receiving end to detect early failure.
//
// Either the caller or the target should free the memory region when it is no
// longer in use. This is initially the callers responsibility, but may be
// handed over to the target by synchronizing using the Mach port.
bool indy_inject(struct indy_info *info, struct indy_error *err);

// Cleanup after indy_inject. Should only be used on the injector, and is
// automatically called when indy_inject fails.
//
// Zero any resources you want to keep alive before calling this function
// (and store their handles elsewhere).
void indy_cleanup(struct indy_info *info);

// Error causes, string constants.
extern const char *indy_couldnt_get_process_info;
extern const char *indy_couldnt_issue_sandbox_extension;
extern const char *indy_couldnt_get_task_port;
extern const char *indy_couldnt_allocate_memory_in_target;
extern const char *indy_couldnt_set_memory_permissions;
extern const char *indy_couldnt_map_target_memory;
extern const char *indy_couldnt_allocate_mach_port;
extern const char *indy_couldnt_extract_port_receive_right;
extern const char *indy_couldnt_insert_port_send_right;
extern const char *indy_couldnt_iterate_target_memory;
extern const char *indy_couldnt_locate_dynamic_loader;
extern const char *indy_couldnt_locate_image_info;
extern const char *indy_couldnt_read_image_info;
extern const char *indy_couldnt_locate_symbols;
extern const char *indy_couldnt_create_thread_in_target;

#endif
