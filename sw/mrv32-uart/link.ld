OUTPUT_ARCH( "riscv" )
ENTRY(_start)

MEMORY {
  RAM (rwx): ORIGIN = 0x80000000, LENGTH = 8k
}

SECTIONS
{
  . = 0x80000000;
  .text.init : { *(.text.init) } > RAM
  . = ALIGN(0x1000);
  .tohost : { *(.tohost) } > RAM
  . = ALIGN(0x1000);
  .text : { *(.text) } > RAM
  . = ALIGN(0x1000);
  .data : { *(.data) } > RAM
  .bss : { *(.bss) } > RAM
  _end = .;
}

