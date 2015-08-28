# inject_dylib, https://github.com/stephank/inject_dylib
# Copyright (c) 2015 Stéphan Kochen
# See the README.md for license details.

.text
.align 4


# Entry-point.
entry:

  subl $32, %esp

  # Get the base address.
  call entry_hop
entry_hop:
  popl %ebx
  subl $entry_hop, %ebx

  # Setup thread-specific data region.
  movl $0x3, %eax                           # thread_fast_set_cthread_self
  movl d_tsd(%ebx), %edx                    # self
  movl %edx, 4(%esp)
  int $0x82

  # Fake the main thread for the purpose of pthreads.
  call *_pthread_main_thread_np(%ebx)
  movl %eax, %gs:0                          # __TSD_THREAD_SELF

  # Create a proper pthread.
  leal 16(%esp), %edx                       # thread
  movl %edx, 0(%esp)
  movl $0, 4(%esp)                          # attr
  leal main(%ebx), %edx                     # start_routine
  movl %edx, 8(%esp)
  movl %ebx, 12(%esp)                       # arg
  call *_pthread_create(%ebx)
  cmpl $0, %eax
  je entry_end

  # Wait for the pthread to finish.
  movl 16(%esp), %edx                       # thread
  movl %edx, 0(%esp)
  movl $0, 4(%esp)                          # value_ptr
  call *_pthread_join(%ebx)

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
  subl $24, %esp

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
  movl %eax, 16(%esp)

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
  movl %eax, d_exit_status(%ebx)

main_unload:

  # Close library.
  movl 16(%esp), %edx                       # handle
  movl %edx, 0(%esp)
  call *_dlclose(%ebx)

main_end:

  movl $0, %eax
  addl $24, %esp
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

# struct indy_target_info
d_dylib_token: .long 0

d_tsd: .long 0

d_exit_status: .long 0

_dlopen: .long 0
_dlsym: .long 0
_dlclose: .long 0

_mach_thread_self: .long 0
_thread_terminate: .long 0

_pthread_main_thread_np: .long 0
_pthread_create: .long 0
_pthread_join: .long 0

_sandbox_extension_consume: .long 0
