// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#include "indy_private.h"

#include <errno.h>
#include <libproc.h>
#include <mach/mach.h>

#include "sandbox_private.h"

bool indy_inject(struct indy_info *info, struct indy_error *err)
{
    bool ok = true;
    int iret;
    kern_return_t kret;
    struct indy_private p = { .info = info };

    info->task = MACH_PORT_NULL;
    info->dylib_token = NULL;

    struct proc_bsdshortinfo pi;
    if (ok) {
        // Grab process info to determine 32 or 64 bit.
        iret = proc_pidinfo(info->pid, PROC_PIDT_SHORTBSDINFO, 0,
                               &pi, PROC_PIDT_SHORTBSDINFO_SIZE);
        ok = (iret == PROC_PIDT_SHORTBSDINFO_SIZE);
        if (!ok)
            indy_set_error(err, indy_couldnt_get_process_info, errno);
    }

    if (ok) {
        // Get a sandbox extension token.
        info->dylib_token = sandbox_extension_issue_file(APP_SANDBOX_READ, info->dylib_path, 0);
        ok = (info->dylib_token != NULL);
        if (!ok)
            indy_set_error(err, indy_couldnt_issue_sandbox_extension, 0);
    }

    // Get the target task port.
    if (ok) {
        kret = task_for_pid(mach_task_self(), info->pid, &info->task);
        ok = indy_set_error_ifneq(err, indy_couldnt_get_task_port, kret, KERN_SUCCESS);
    }

    // Allocate a block of memory.
    mach_vm_address_t local_region_addr = 0;
    if (ok) {
        kret = mach_vm_allocate(mach_task_self(), &local_region_addr, INDY_ALLOC_SIZE, 1);
        ok = indy_set_error_ifneq(err, indy_couldnt_allocate_memory_in_target, kret, KERN_SUCCESS);
    }

    // Map the memory anywhere into the target address space.
    if (ok) {
        info->region_addr = 0;
        vm_prot_t cur_protection, max_protection;
        kret = mach_vm_remap(info->task, &info->region_addr, INDY_ALLOC_SIZE, 4, 1,
                             mach_task_self(), local_region_addr, 0,
                             &cur_protection, &max_protection, VM_INHERIT_NONE);
        ok = indy_set_error_ifneq(err, indy_couldnt_map_target_memory, kret, KERN_SUCCESS);
    }

    // Make the memory executable.
    if (ok) {
        p.local_region = (void *) local_region_addr;

        kret = mach_vm_protect(info->task, info->region_addr, INDY_ALLOC_SIZE,
                               0, VM_PROT_EXECUTE | VM_PROT_WRITE | VM_PROT_READ);
        ok = indy_set_error_ifneq(err, indy_couldnt_set_memory_permissions, kret, KERN_SUCCESS);
    }

    // Create a port in the priv.task task.
    if (ok) {
        kret = mach_port_allocate(info->task, MACH_PORT_RIGHT_RECEIVE, &p.target_port);
        ok = indy_set_error_ifneq(err, indy_couldnt_allocate_mach_port, kret, KERN_SUCCESS);
    }

    // Nab the receive rights from the target task.
    // FIXME: Not sure if this fully destroys the port in the target. If it does,
    // then this is a race condition, because the port name could've been
    // acquired by something else before the next step.
    if (ok) {
        mach_msg_type_name_t port_type;
        kret = mach_port_extract_right(info->task, p.target_port, MACH_MSG_TYPE_MOVE_RECEIVE,
                                       &info->port, &port_type);
        ok = indy_set_error_ifneq(err, indy_couldnt_extract_port_receive_right, kret, KERN_SUCCESS);
    }

    // Give the target task send rights.
    if (ok) {
        kret = mach_port_insert_right(info->task, p.target_port, info->port, MACH_MSG_TYPE_MAKE_SEND);
        ok = indy_set_error_ifneq(err, indy_couldnt_insert_port_send_right, kret, KERN_SUCCESS);
    }

    // Continue with architecture specifics.
    if (ok) {
        if (pi.pbsi_flags & PROC_FLAG_LP64)
            ok = indy_inject_x86_64(&p, err);
        else
            ok = indy_inject_i386(&p, err);
    }

    // Unmap the memory block from our address space.
    if (local_region_addr != 0)
        mach_vm_deallocate(mach_task_self(), local_region_addr, INDY_ALLOC_SIZE);

    // Cleanup.
    if (!ok) {
        if (p.target_port != MACH_PORT_NULL)
            mach_port_destroy(info->task, p.target_port);
        indy_cleanup(info);
    }
    return ok;
}

void indy_cleanup(struct indy_info *info)
{
    if (info->port != MACH_PORT_NULL) {
        mach_port_destroy(mach_task_self(), info->port);
        info->port = MACH_PORT_NULL;
    }

    if (info->task != MACH_PORT_NULL && info->region_addr != 0) {
        vm_deallocate(info->task, info->region_addr, info->region_size);
        info->region_addr = 0;
    }

    if (info->dylib_token != NULL) {
        sandbox_extension_release(info->dylib_token);
        free(info->dylib_token);
        info->dylib_token = NULL;
    }

    if (info->task != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), info->task);
        info->task = MACH_PORT_NULL;
    }
}
