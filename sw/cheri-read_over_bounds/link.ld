SECTIONS
{
  . = 0x00000000;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss  : { *(.bss) }

  /* stack at the end of memory
   * for initialization of the stack pointer capability
   */
  . = 32M;
  .stack : {
    _stack_top = .;
  }
}
ENTRY(_start)
