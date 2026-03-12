# Example software tests

This test suite contains sample software and their expected output.

## Layout

Each test case has its own subdirectory. Each directory contains the
following files:

* `*.{c,S}`: Source code for the test program.
* `CMakeLists.txt`: Instructions for compiling the test program which
  **must** have the same name as the test itself, i.e. the directory
  name.
* `opts`: Additional command line flags which should be passed to the
  simulator.
* `output`: Expected output of the simulator for the test program.
* post-process.sh`: Script which receives the simulator output
  on standard input and can be used to transform it before comparing it
  with the `output` file.

Tests prefixed with `-mc` are executed with a multicore simulator.
