# inject_dylib, https://github.com/stephank/inject_dylib
# Copyright (c) 2015 Stéphan Kochen
# See the README.md for license details.

.text
.align 4


# Entry-point.
entry:

  subl $0x10, %esp

  # Get the base address.
  call entry_hop
entry_hop:
  popl %ebx
  subl $entry_hop, %ebx

  # Setup thread-specific data.
  movl $0x3, %eax                           # thread_fast_set_cthread_self
  movl d_tsd(%ebx), %edx                    # self
  movl %edx, 4(%esp)
  int $0x82

  # Create a proper pthread.
  leal d_thread(%ebx), %edx                 # thread
  movl %edx, 0(%esp)
  movl $0, 4(%esp)                          # attr
  leal main(%ebx), %edx                     # start_routine
  movl %edx, 8(%esp)
  movl %ebx, 12(%esp)                       # arg
  call *_pthread_create(%ebx)
  cmpl $0, %eax
  je entry_end

  # Failed, so close the mach port.
  call close_port

entry_end:

  # Terminate this thread.
  call *_mach_thread_self(%ebx)
  movl %eax, 0(%esp)                        # target_thread
  call *_thread_terminate(%ebx)

  # Should never reach, crash.
  ud2


# Entry-point of pthread.
main:

  pushl %ebp
  movl %esp, %ebp
  subl $0x8, %esp

  # Grab the base address.
  movl 8(%ebp), %ebx

  # Consume the sandbox extension.
  movl d_dylib_token(%ebx), %edx            # token
  movl %edx, 0(%esp)
  call *_sandbox_extension_consume(%ebx)

  # Open library.
  movl d_dylib_path(%ebx), %edx             # path
  movl %edx, 0(%esp)
  movl $4, 4(%esp)                          # mode = RTLD_LOCAL
  call *_dlopen(%ebx)
  cmpl $0, %eax
  je main_end
  movl %eax, d_dylib_handle(%ebx)

  # Find entry-point.
  movl %eax, 0(%esp)                        # handle
  movl d_dylib_entry_symbol(%ebx), %edx     # symbol
  movl %edx, 4(%esp)
  call *_dlsym(%ebx)
  cmpl $0, %eax
  je main_unload

  # Call entry-point.
  leal info(%ebx), %edx                     # info
  movl %edx, 0(%esp)
  call *%eax

main_unload:

  # Close library.
  movl d_dylib_handle(%ebx), %edx           # handle
  movl %edx, 0(%esp)
  call *_dlclose(%ebx)

main_end:

  # Clean-up the mach port.
  call close_port

  addl $0x8, %esp
  popl %ebp
  ret


# Helper used to close the mach port.
close_port:

  pushl %ebp
  movl %esp, %ebp
  subl $0x8, %esp

  call *_mach_task_self(%ebx)
  movl %eax, 0(%esp)                        # task
  movl d_port(%ebx), %edx                   # name
  movl %edx, 4(%esp)
  call *_mach_port_deallocate(%ebx)

  addl $0x8, %esp
  popl %ebp
  ret


.align 4
info:

# struct indy_info
d_pid: .long 0

d_dylib_path: .long 0
d_dylib_entry_symbol: .long 0

d_user_data: .long 0
d_user_data_size: .quad 0

d_dylib_token: .long 0

d_region_addr: .quad 0
d_region_size: .quad 0

d_port: .long 0

d_thread: .long 0

d_dylib_handle: .long 0

# struct indy_target_info
d_tsd: .long 0

_dlopen: .long 0
_dlsym: .long 0
_dlclose: .long 0

_mach_task_self: .long 0
_mach_port_deallocate: .long 0
_mach_thread_self: .long 0
_thread_terminate: .long 0

_pthread_create: .long 0

_sandbox_extension_consume: .long 0
