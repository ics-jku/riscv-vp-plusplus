Start Eclipse

File -> New -> Makefile Project with Existing Code

    Select the software folder containing the Makefile

Run -> Debug Configurations ...

    C/C++ Remote Application (List Entry on the left side): Click "New" Button

    Main Tab
        - C/C++ Application Name: Enter name, typically "main"
        - Using GDB (DSF) Automatic Remote Launcher: Click "Select other"
            -> Use configuration specific settings
            -> GDB (DSF) Manual Remote Debug Launcher

    Debugger Tab
        - GDB debugger: riscv32-unknown-elf-gdb   [Sub-tab Main]
        - Connection: Port Number 5005            [Sub-tab Connection]

    Click "Debug" Button (lower right corner) to start debugging
        -> Launch the VP in debug mode first: riscv-vp --debug-mode main
        -> Perhaps it is necessary to refresh the Eclipse project view in case the executable (typically main) cannot be found (the "Debug" button is greyed out in this case)
