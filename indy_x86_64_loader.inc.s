# inject_dylib, https://github.com/stephank/inject_dylib
# Copyright (c) 2015 Stéphan Kochen
# See the README.md for license details.

.text
.align 4


# Entry-point.
entry:

  # Setup thread-specific data.
  movq $0x3000003, %rax                     # thread_fast_set_cthread_self64
  movq d_tsd(%rip), %rdi                    # self
  syscall

  # Create a proper pthread.
  leaq d_thread(%rip), %rdi                 # thread
  movq $0, %rsi                             # attr
  leaq main(%rip), %rdx                     # start_routine
  movq $0, %rcx                             # arg
  call *_pthread_create(%rip)
  cmpl $0, %eax
  je entry_end

  # Failed, so close the mach port.
  call close_port

entry_end:

  # Terminate this thread.
  call *_mach_thread_self(%rip)
  movl %eax, %edi                           # target_thread
  call *_thread_terminate(%rip)

  # Should never reach, crash.
  ud2


# Entry-point of pthread.
main:

  pushq %rbp
  movq %rsp, %rbp

  # Consume the sandbox extension.
  movq d_dylib_token(%rip), %rdi            # token
  int3
  call *_sandbox_extension_consume(%rip)

  # Open library.
  movq d_dylib_path(%rip), %rdi             # path
  movl $4, %esi                             # mode = RTLD_LOCAL
  call *_dlopen(%rip)
  cmpq $0, %rax
  je main_end
  movq %rax, d_dylib_handle(%rip)

  # Find entry-point.
  movq %rax, %rdi                           # handle
  movq d_dylib_entry_symbol(%rip), %rsi     # symbol
  call *_dlsym(%rip)
  cmpq $0, %rax
  je main_unload

  # Call entry-point.
  leaq info(%rip), %rdi                     # info
  call *%rax

main_unload:

  # Close library.
  movq d_dylib_handle(%rip), %rdi           # handle
  call *_dlclose(%rip)

main_end:

  # Clean-up the mach port.
  call close_port

  popq %rbp
  ret


# Helper used to close the mach port.
close_port:

  pushq %rbp
  movq %rsp, %rbp

  call *_mach_task_self(%rip)
  movl %eax, %edi                           # task
  movl d_port(%rip), %esi                   # name
  call *_mach_port_deallocate(%rip)

  popq %rbp
  ret


.align 4
info:

# struct indy_info
d_pid: .long 0
.long 0

d_dylib_path: .quad 0
d_dylib_entry_symbol: .quad 0

d_user_data: .quad 0
d_user_data_size: .quad 0

d_dylib_token: .quad 0

d_region_addr: .quad 0
d_region_size: .quad 0

d_port: .long 0
.long 0

d_thread: .quad 0

d_dylib_handle: .quad 0

# struct indy_target_info
d_tsd: .quad 0

_dlopen: .quad 0
_dlsym: .quad 0
_dlclose: .quad 0

_mach_task_self: .quad 0
_mach_port_deallocate: .quad 0
_mach_thread_self: .quad 0
_thread_terminate: .quad 0

_pthread_create: .quad 0

_sandbox_extension_consume: .quad 0
