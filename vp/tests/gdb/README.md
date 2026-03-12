# Tests for GDB stub

This test suite contains tests for the `gdb-mc` stub implementation.

## Layout

Each test case has its own subdirectory. Each directory contains at
least the following files:

* `*.{c,S}`: Source code for the test program.
* `CMakeLists.txt`: Instructions for compiling the test program which
  **must** have the same name as the test itself, i.e. the directory
  name.
* `gdb-cmds`: Commands to be executed by the `gdb(1)` client.
* `output`: Expected output of the `gdb(1)` program.
* `post-process.sh`: Script which receives the `gdb(1)` output
   on standard input and can be used to transform it before comparing
   it with the `output` file.

Tests prefixed with `-mc` are executed with a multicore simulator.

## Configuration

The following environment variables can be set:

* `GDB_DEBUG_PROG`: Name of the `gdb(1)` binary with RISC-V support.
* `GDB_DEBUG_PORT`: TCP port to use for testing the GDB protocol.
