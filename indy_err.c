// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#include "indy_private.h"

const char *indy_couldnt_get_process_info =
    "Couldn't get process info (proc_pidinfo error %d)\n";

const char *indy_couldnt_issue_sandbox_extension =
    "Couldn't issue sandbox extension (sandbox_extension_issue_file error)\n";

const char *indy_couldnt_get_task_port =
    "Couldn't get task port (task_for_pid error %d)\n";

const char *indy_couldnt_allocate_memory_in_target =
    "Couldn't allocate memory in target (mach_vm_allocate error %d)\n";

const char *indy_couldnt_map_target_memory =
    "Couldn't map target memory (mach_vm_remap error %d)\n";

const char *indy_couldnt_set_memory_permissions =
    "Couldn't set memory permissions (mach_vm_protect error %d)\n";

const char *indy_couldnt_create_thread_in_target =
    "Couldn't create thread in target (thread_create_running error %d)\n";

const char *indy_couldnt_locate_dynamic_loader =
    "Couldn't locate the dynamic loader in target memory\n";

const char *indy_couldnt_iterate_target_memory =
    "Couldn't iterate target memory (mach_vm_region_recurse error %d)\n";

const char *indy_couldnt_locate_image_info =
    "Couldn't locate the target image info data\n";

const char *indy_couldnt_read_image_info =
    "Couldn't read the target image info data (mach_vm_read_overwrite error %d)\n";

const char *indy_couldnt_locate_symbols =
    "Couldn't locate all required loader symbols in target memory\n";

const char *indy_couldnt_set_target_thread_state =
    "Couldn't set target thread state (thread_set_state error %d)\n";

const char *indy_couldnt_create_notification_port =
    "Couldn't create notification port (mach_port_allocate error %d)\n";

const char *indy_couldnt_request_thread_end_notification =
    "Couldn't request thread end notification (thread_set_state error %d)\n";

const char *indy_couldnt_start_target_thread =
    "Couldn't start target thread (thread_resume error %d)\n";

const char *indy_couldnt_read_thread_end_notification =
    "Couldn't read thread end notification (mach_msg error %d)\n";

const char *indy_couldnt_read_exit_status =
    "Couldn't read the exit status (mach_vm_read_overwrite error %d)\n";
