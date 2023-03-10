# GUI-VP -- RISC-V based Virtual Prototype (VP) for graphical application development

*GUI-VP* is a greatly extended and improved open-source [RISC-V VP](https://github.com/agra-uni-bremen/riscv-vp) that enables the simulation of interactive graphical Linux applications.
It was created at the [Institute for Complex Systems](https://ics.jku.at/), Johannes Kepler University, Linz.
All further work on *GUI-VP* will take place here in *RISCV-VP++*

The main features of *GUI-VP* are:
 * Support for 32 and 64 bit Linux (RV32 and RV64 VPs)
 * Matched real- and simulation-time for correct timing behavior in interactions (Light-weight Real-Time CLINT)
 * Graphics Output (800x480 RGB565 Framebuffer, VNC)
 * Mouse Input (VNC)
 * Keyboard Input (VNC)
 * Efficient boot (Memory-mapped file for rootfs)
 * Persistent storage (Memory-mapped file for data)

**We strongly recommend using *GUI-VP* with [GUI-VP Kit](https://github.com/ics-jku/GUI-VP_Kit).
*GUI-VP Kit* provides an easy-to-use build system and experimentation platform for *GUI-VP* and Linux.**

*GUI-VP Kit* and *GUI-VP* were first introduced in 2023 at the *ACM Great Lakes Symposium on VLSI*, with
[Manfred Schlägl and Daniel Große. GUI-VP Kit: A RISC-V VP meets Linux graphics - enabling interactive graphical application development. In GLSVLSI, 2023.
](https://ics.jku.at/files/2023GLSVLSI_GUI-VP_Kit.pdf)

BibTex entry:
```
@inproceedings{SG:2023,
  author =        {Manfred Schl{\"{a}}gl and Daniel Gro{\ss}e},
  booktitle =     {ACM Great Lakes Symposium on VLSI},
  title =         {{GUI-VP Kit}: A {RISC-V} {VP} Meets {Linux} Graphics
                   - Enabling Interactive Graphical Application
                   Development},
  year =          {2023},
}
```
