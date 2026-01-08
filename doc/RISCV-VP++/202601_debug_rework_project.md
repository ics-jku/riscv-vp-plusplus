# GDB Project ModernSystemDesign

January 2026, Alexander PICHLER (minor updates by Manfred SCHLAEGL)

The following documents the 3 main enhancements I (Alexander Pichler) made to the GDB implementation of the RISC-V VP++ in the course of the Practical Exam for the course  *Practical Introduction to Modern System Design with C++*. First, I will list all the files that are relevant. Then, I will go through the 3 changes one by one, explaining for each the motivation, the most important pieces of code and how to reproduce the change in behavior compared to the predecessor version. I will only explain the shifts in architecture and not every implementation detail. The specification I followed for these changes is available [here](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Remote-Protocol.html).

### Relevant Files

The following is a list of all files that are relevant. Later in the document, I will refer to the files only by their name, for instance `gdb_runner.cpp`.

- `vp/src/core/common/gdb-mc/gdb_runner.cpp`
- `vp/src/core/common/gdb-mc/gdb_server.h`
- `vp/src/core/common/gdb-mc/gdb_server.cpp`
- `vp/src/core/common/gdb-mc/handler.cpp`
- `vp/src/core/common/gdb-mc/libgdb/include/libgdb/parser1.h`
- `vp/src/core/common/gdb-mc/libgdb/parser1.c`
- `vp/src/platform/common/options.cpp`
- `vp/src/platform/common/options.h`
- `vp/src/platform/*/*main.cpp`

### Enhancement 1: Fixing vCont when simulating multiple cores

One issue I noticed is the following: Say you simulate multiple cores and the simulation halts, for instance because a breakpoint was hit. If you now want to step one of the cores, the debugger becomes unresponsive. You can try this by first building an old version of `VP++` (from before this change), then adding in `sw/basic-multicore/Makefile` both to `CFLAGS` `-g3` and to `VP_FLAGS` `--debug-mode`. Then run `make sim` in this directory. In a different terminal in the same directory, run `riscv32-unknown-elf-gdb`, `file main`, `target remote:5005`, `b main.c:24`, `c`, `delete 1` and `s`. You would expect the core that hit the breakpoint to execute one LOC and the debugger responding. However, what actually happens is that the core that hit the breakpoint executes one LOC, but the other core just continues and the debugger never responds.

The issue is the implementation of the `vCont` command. The gdb client tells the stub roughly the following via the `vCont` command: *Step this one hart, continue all others*. The handler in `handler.cpp` then does the following: It takes the first element in the `vCont` linked list, which says *Step this one hart*, and does exactly that. It then takes the second element in the `vCont` linked list, which says *Continue all others*. The handler does this as well and then waits until the first hart finishes execution. However, this never happens, and thus the stub never responds. In my opinion, the correct implementation of `vCont` and the all-stop mode here is that when the hart that got stepped finishes execution, all other harts should stop too and the stub should respond.

To implement this behavior, I changed the following: Instead of `GDBServer::vCont` in `handler.cpp` handling each element in the `vCont` linked list one by one by calling `GDBServer::run_threads(std::vector<debug_target_if *> hartsrun, bool single)` in `gdb_server.cpp` for each element, we first collect all harts from all the elements. We then call the new `GDBServer::run_all_harts(std::vector<debug_target_if *> harts_to_run)` method once by passing the collection of all harts. This method then makes sure that when any hart stops, for instance because it was only supposed to step, all harts get stopped, correctly implementing the desired behavior.

To see the new behavior, you can simply follow the steps outlined above. After the last command `s`, you will see that the GDB stub responds and that targeted thread executed one LOC. You can switch harts via `thread 1` and `thread 2` and try this for both harts. It is important to mention though that the other thread could have executed either 0 or many instructions in the meantime due to SystemC scheduling, which in my opinion is not a problem according to the spec.

### Enhancement 2: Adding Ctrl-C (Debugger Interrupt)

Next, I noticed that using `Ctrl-C` to interrupt a running program does not work, which is however part of the [specification](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Interrupts.html#Interrupts). To try this, build an old `VP++` version (from before this change). From now on, we are always gonna use the multicore example, simply because it is the more general case and it shows these enhancements also work for multicore systems. In `sw/basic-multicore/bootstrap.S`, I added the lines `spin:` and `j spin` after `jal main` to make the harts spin after each finishing their task. Then follow the steps outlined above, but instead of setting a breakpoint, simply run `c`. Then try to halt the running program by pressing `Ctrl-C`. This will not work since it is not implemented.

To introduce this functionality, I first added a new variation `GDB_KIND_INTERRUPT` to the enum `gdb_kind_t` in `parser1.h`. In `parser1.c`, I added code so that we get this kind of packet when `0x03` appear on the TCP connection. I then had to refactor `GDBServer::dispatch` in `gdb_server.cpp` so that we directly call `interruptEvent.notify()` in case of an interrupt, which bypasses `pktq` to give the interrupt priority. In the new `GDBServer::run_all_harts` method, we then extended the condition where we wait for any hart to stop to `sc_core::sc_event_or_list all_events = allharts | interruptEvent`. This means that an interrupt gets handled in exactly the same way as any hart stopping would get handled, leading to all harts getting stopped.

To test the behavior, simply follow the above steps again, except that you have to build a recent version `VP++` (including the change). You can now halt the program at any time, including in the endless loop at the end.

### Enhancement 3: Implementing gdb halt SysC sim on wait (e.g. breakpoint)

Last but not least, I modified the gdb debug interface to halt the whole SystemC simulation when the cores are halted (e.g. breakpoint) and added an optional switch `--debug-cont-sim-on-wait` for switching back to the old behavior (continue the SystemC simulation on debug halt).
For a motivating example, try running `sw/simple-sensor` with the old debugger behavior (`--debug-cont-sim-on-wait`). Add both to `CFLAGS` `-g3` and to `VP_FLAGS` `--debug-mode` and `--debug-cont-sim-on-wait` in `sw/simple-sensor/Makefile`. In one terminal in this directory, run `make sim`. In another terminal in the same directory, run `riscv32-unknown-elf-gdb`, `file main`, `target remote:5005`, `b main.c:13` and `b main.c:20` and then repeatedly `c`. You will notice that we only hit the breakpoint in line 13. This is because while the cores are paused, the simulation continues and the peripheral sensor continues to trigger interrupts.
Removing the flag  `--debug-cont-sim-on-wait` switches to the new default behavior which halts the whole SystemC simulation when a hart halts, i.e. repeating the above experiment without this switch achieves the behavior where also the breakpoint in line 20 gets hit.

To implement this new behavior, I added an option to `options.h` and `options.cpp`. In the top-levels, this option gets passed to the GDB server. The crucial change in the `GDBServer::run` method in `gdb_server.cpp` is the following: When this flag is `true` (old behavior) and the loop runs out of packets to process, we simply call `sc_core::wait(asyncEvent)`, as before. Since we wait on the SystemC level, other SystemC threads, representing for instance peripherals, continue to run. When this flag is set to `false` (new behavior), instead of doing a SystemC wait, we do an OS level wait using `cv.wait(lock)`. Since this blocks the whole system thread, the SystemC simulation is halted. In `GDBServer::dispatch`, we call the corresponding wakeup method, either `asyncEvent.notify()` or `cv.notify_one()`. There is one crucial constraint the code currently fulfills that makes this work properly: All the other handlers, apart from `vCont`, never do a SystemC wait. If they did, control would be 'leaked', meaning that other SystemC threads could run while, for instance, reading from registers.

To test this behavior, simply repeat the steps above without `--debug-cont-sim-on-wait` to the `VP_FLAGS`. You will see that both breakpoints get triggered and the simulation actually progresses properly. This confirms the SystemC simulation gets halted.







