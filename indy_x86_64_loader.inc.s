# inject_dylib, https://github.com/stephank/inject_dylib
# Copyright (c) 2015 Stéphan Kochen
# See the README.md for license details.

.text
.align 4


# Entry-point.
entry:

  subq $16, %rsp

  # Setup thread-specific data region.
  movq $0x3000003, %rax                     # thread_fast_set_cthread_self64
  movq d_tsd(%rip), %rdi                    # self
  syscall

  # Fake the main thread for the purpose of pthreads.
  call *_pthread_main_thread_np(%rip)
  movq %rax, %gs:0                          # __TSD_THREAD_SELF

  # Create a proper pthread.
  leaq 0(%rsp), %rdi                        # thread
  movq $0, %rsi                             # attr
  leaq main(%rip), %rdx                     # start_routine
  movq $0, %rcx                             # arg
  call *_pthread_create(%rip)
  cmpl $0, %eax
  jne entry_end

  # Wait for the pthread to finish.
  movq 0(%rsp), %rdi                        # thread
  movq $0, %rsi                             # value_ptr
  call *_pthread_join(%rip)

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
  subq $16, %rsp

  # Consume the sandbox extension.
  movq d_dylib_token(%rip), %rdi            # token
  call *_sandbox_extension_consume(%rip)

  # Open library.
  movq d_dylib_path(%rip), %rdi             # path
  movl $4, %esi                             # mode = RTLD_LOCAL
  call *_dlopen(%rip)
  cmpq $0, %rax
  je main_end
  movq %rax, 0(%rsp)

  # Find entry-point.
  movq %rax, %rdi                           # handle
  movq d_dylib_entry_symbol(%rip), %rsi     # symbol
  call *_dlsym(%rip)
  cmpq $0, %rax
  je main_unload

  # Call entry-point.
  movq d_user_data(%rip), %rdi              # user_data
  call *%rax
  movl %eax, d_exit_status(%rip)

main_unload:

  # Close library.
  movq 0(%rsp), %rdi                        # handle
  call *_dlclose(%rip)

main_end:

  movq $0, %rax
  addq $16, %rsp
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

# struct indy_target_info
d_dylib_token: .quad 0

d_tsd: .quad 0

d_exit_status: .long 0
.long 0

_dlopen: .quad 0
_dlsym: .quad 0
_dlclose: .quad 0

_mach_thread_self: .quad 0
_thread_terminate: .quad 0

_pthread_main_thread_np: .quad 0
_pthread_create: .quad 0
_pthread_join: .quad 0

_sandbox_extension_consume: .quad 0
