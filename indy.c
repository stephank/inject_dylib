// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#include "indy_private.h"

#include <errno.h>
#include <libproc.h>
#include <mach/mach.h>

#include "sandbox_private.h"

void indy_inject(const struct indy_info *info, struct indy_result *res)
{
    bool ok = true;
    int iret;
    kern_return_t kret;
    mach_msg_return_t mret;
    struct indy_private p = { .info = info, .res = res };

    // Clear error.
    res->error = NULL;

    // Grab process info to determine 32 or 64 bit.
    struct proc_bsdshortinfo pi;
    if (ok) {
        iret = proc_pidinfo(info->pid, PROC_PIDT_SHORTBSDINFO, 0,
                               &pi, PROC_PIDT_SHORTBSDINFO_SIZE);
        ok = (iret == PROC_PIDT_SHORTBSDINFO_SIZE);
        if (!ok)
            indy_set_error(res, indy_couldnt_get_process_info, errno);
    }

    // Get a sandbox extension token.
    if (ok) {
        p.dylib_token = sandbox_extension_issue_file(APP_SANDBOX_READ, info->dylib_path, 0, 0);
        ok = (p.dylib_token != NULL);
        if (!ok)
            indy_set_error(res, indy_couldnt_issue_sandbox_extension, 0);
    }

    // Get the target task port.
    if (ok) {
        kret = task_for_pid(mach_task_self(), info->pid, &p.task);
        ok = indy_set_error_ifneq(res, indy_couldnt_get_task_port, kret, KERN_SUCCESS);
    }

    // Allocate a block of memory.
    mach_vm_address_t local_region_addr = 0;
    if (ok) {
        kret = mach_vm_allocate(mach_task_self(), &local_region_addr, INDY_ALLOC_SIZE, 1);
        ok = indy_set_error_ifneq(res, indy_couldnt_allocate_memory_in_target, kret, KERN_SUCCESS);
    }

    // Map the memory anywhere into the target address space.
    if (ok) {
        vm_prot_t cur_protection, max_protection;
        kret = mach_vm_remap(p.task, &p.region_addr, INDY_ALLOC_SIZE, 4, 1,
                             mach_task_self(), local_region_addr, 0,
                             &cur_protection, &max_protection, VM_INHERIT_NONE);
        ok = indy_set_error_ifneq(res, indy_couldnt_map_target_memory, kret, KERN_SUCCESS);
        if (ok)
            p.local_region = (void *) local_region_addr;
    }

    // Make the memory executable.
    if (ok) {
        kret = mach_vm_protect(p.task, p.region_addr, INDY_ALLOC_SIZE,
                               0, VM_PROT_EXECUTE | VM_PROT_WRITE | VM_PROT_READ);
        ok = indy_set_error_ifneq(res, indy_couldnt_set_memory_permissions, kret, KERN_SUCCESS);
    }

    // Create the thread.
    if (ok) {
        kret = thread_create(p.task, &p.target_thread);
        ok = indy_set_error_ifneq(res, indy_couldnt_create_thread_in_target, kret, KERN_SUCCESS);
    }

    // Setup the info structure in the target and thread state.
    if (ok) {
        if (pi.pbsi_flags & PROC_FLAG_LP64)
            ok = indy_setup_x86_64(&p);
        else
            ok = indy_setup_i386(&p);
    }

    // Unmap the memory block from our address space.
    if (local_region_addr != 0)
        mach_vm_deallocate(mach_task_self(), local_region_addr, INDY_ALLOC_SIZE);

    // Create a receive port for notifications.
    mach_port_t notify_port = MACH_PORT_NULL;
    if (ok) {
        kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notify_port);
        ok = indy_set_error_ifneq(res, indy_couldnt_create_notification_port, kret, KERN_SUCCESS);
    }

    // Request dead name notification for the thread, signalling the thread was deallocated.
    mach_port_t notify_prev = MACH_PORT_NULL;
    if (ok) {
        kret = mach_port_request_notification(mach_task_self(), p.target_thread, MACH_NOTIFY_DEAD_NAME,
                                              1, notify_port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &notify_prev);
        ok = indy_set_error_ifneq(res, indy_couldnt_request_thread_end_notification, kret, KERN_SUCCESS);
    }

    // Deallocate the previous notification port.
    if (notify_prev != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), notify_prev);

    // Start the thread.
    if (ok) {
        kret = thread_resume(p.target_thread);
        ok = indy_set_error_ifneq(res, indy_couldnt_start_target_thread, kret, KERN_SUCCESS);
    }

    // If we couldn't start the thread, clean up resources we allocated in the
    // target at this point. We don't this if the thread DID start, because
    // that'd crash the target. Instead, leak stuff in the unlikely scenario.
    if (!ok) {
        if (p.target_thread != MACH_PORT_NULL)
            thread_terminate(p.target_thread);

        if (p.region_addr != 0)
            mach_vm_deallocate(p.task, p.region_addr, p.region_size);

        if (p.dylib_token != NULL)
            sandbox_extension_release(p.dylib_token);
    }

    // Free sandbox extension memory.
    if (p.dylib_token != NULL)
        free(p.dylib_token);

    // Wait for the thread to halt.
    if (ok) {
        mach_dead_name_notification_t msg;
        mret = mach_msg(&msg.not_header, MACH_RCV_MSG, 0, sizeof(msg),
                        notify_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        ok = indy_set_error_ifneq(res, indy_couldnt_read_thread_end_notification, mret, MACH_MSG_SUCCESS);
        if (ok) {
            if (msg.not_header.msgh_id != MACH_NOTIFY_DEAD_NAME)
                ok = indy_set_error(res, indy_couldnt_read_thread_end_notification, MACH_MSG_SUCCESS);
            mach_msg_destroy(&msg.not_header);
        }
    }

    // Get the exit code from the target info structure.
    // This does not alter ok, because we only use it to check if it's safe
    // to deallocate the memory block below. This shouldn't stop that.
    if (ok) {
        mach_vm_size_t exit_status_size;
        kret = mach_vm_read_overwrite(p.task, p.exit_status_addr, sizeof(uint32_t),
                                      (mach_vm_address_t) &res->exit_status, &exit_status_size);
        if (kret != KERN_SUCCESS || exit_status_size != sizeof(res->exit_status))
            indy_set_error(res, indy_couldnt_read_exit_status, kret);
    }

    // Clean up the notify port and thread port.
    if (notify_port != MACH_PORT_NULL)
        mach_port_destroy(mach_task_self(), notify_port);
    if (p.target_thread != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), p.target_thread);

    // Deallocate the memory block now that the loader is done.
    if (ok)
        mach_vm_deallocate(p.task, p.region_addr, p.region_size);

    // Cleanup the task port.
    if (p.task != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), p.task);
}
