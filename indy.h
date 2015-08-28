// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#ifndef indy_h
#define indy_h

#include <stdlib.h>
#include <stdbool.h>
#include <mach/mach.h>

// Info structure provided by the caller.
struct indy_info
{
    // Target PID.
    pid_t pid;

    // Path and entry point of the injected dynamic library.
    const char *dylib_path;
    const char *dylib_entry_symbol;

    // User data to copy to the target process.
    const void *user_data;
    uint64_t user_data_size;
};

// Structure containing inject results.
//
// The errors generated may not be extremely useful to end-users. Ideally,
// common user errors would be documented here, so that the application may
// give useful feedback. For now, it's mostly useful in bug reports.
struct indy_result
{
    // NULL or one of the string constants below. May be printed as
    // a format string along with os_value: `printf(res.error, res.os_value);`
    const char *error;
    int64_t os_value;    // Usually a kern_return_t, but dependant on error.

    // The target procedure exit status.
    uint32_t exit_status;
};

// Entry point signature. Receives a copy of user data, and returns an exit
// status that is communicated in the result structure on the injector.
typedef uint32_t (*indy_entry)(void *user_data);

// Inject into a process. Blocks until the target procedure finishes. When
// successful, the result error field is NULL, and the exit_status field
// contains the return value of the procedure. On failure, error is non-NULL.
//
// All target resources are cleaned up when the target procedure terminates,
// including the copy of the user data. If execution in the target should
// continue, the injected code should dlopen another library and spawn a
// detached thread there.
void indy_inject(const struct indy_info *info, struct indy_result *res);

// Error causes, string constants.
extern const char *indy_couldnt_get_process_info;                   // errno
extern const char *indy_couldnt_issue_sandbox_extension;            // no value
extern const char *indy_couldnt_get_task_port;                      // kern_return_t
extern const char *indy_couldnt_allocate_memory_in_target;          // kern_return_t
extern const char *indy_couldnt_map_target_memory;                  // kern_return_t
extern const char *indy_couldnt_set_memory_permissions;             // kern_return_t
extern const char *indy_couldnt_create_thread_in_target;            // kern_return_t
extern const char *indy_couldnt_locate_dynamic_loader;              // no value
extern const char *indy_couldnt_iterate_target_memory;              // kern_return_t
extern const char *indy_couldnt_locate_image_info;                  // no value
extern const char *indy_couldnt_read_image_info;                    // kern_return_t
extern const char *indy_couldnt_locate_symbols;                     // no value
extern const char *indy_couldnt_set_target_thread_state;            // kern_return_t
extern const char *indy_couldnt_create_notification_port;           // kern_return_t
extern const char *indy_couldnt_request_thread_end_notification;    // kern_return_t
extern const char *indy_couldnt_start_target_thread;                // kern_return_t
extern const char *indy_couldnt_read_thread_end_notification;       // mach_msg_return_t
extern const char *indy_couldnt_read_exit_status;                   // kern_return_t

#endif
