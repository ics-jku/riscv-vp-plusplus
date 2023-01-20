These SW examples demonstrate basic bare-metal applications and applications that use the C-library. In particular, the  applications using the C-library require support for system calls (syscalls). Syscalls are handled in one of two ways in the VP:

 1) By default the VP will trigger a trap and thus jump to the trap/interrupt handler. The handler will detect that the reason is a syscall and re-direct the syscall to the syscall handler (or some other peripheral) through memory mapped I/O. This requires to provide and setup a trap handler. This can be done by providing a custom entry-point that performs the required setup and then jumps to the crt0.S entry code of the C-library (which in turn will call the main function). The *printf* SW example demonstrates this case.

 2) With the "--intercept-syscalls" command line argument, the VP will directly intercept and handle syscalls, hence it is not necessary to setup a trap handler. Thus, no designated startup code (i.e. bootstrap.S) is required to run applications that use the C-library. For example see the *c++-lib* SW example.

The provided SW examples demonstrate both approaches for dealing with syscalls.

The VP also supports the FreeRTOS (see https://github.com/agra-uni-bremen/riscv-freertos) and Zephyr (see https://www.zephyrproject.org/ - use the Zephyr *hifive* board option when building Zephyr applications and use the *hifive-vp* configuration for executing them) operating systems.
