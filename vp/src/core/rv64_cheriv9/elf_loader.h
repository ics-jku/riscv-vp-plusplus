#pragma once

/*
 * TODO: cleanup duplication with rv64/elf_loader.h
 */

#include "core/common/elf_loader.h"

namespace cheriv9::rv64 {

// see: "ELF-64 Object File Format" document for ELF64 type definitions

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

constexpr unsigned ELF_NIDENT = 16;

typedef struct {
	unsigned char e_ident[ELF_NIDENT]; /* ELF identification */
	Elf64_Half e_type;                 /* Object file type */
	Elf64_Half e_machine;              /* Machine type */
	Elf64_Word e_version;              /* Object file version */
	Elf64_Addr e_entry;                /* Entry point address */
	Elf64_Off e_phoff;                 /* Program header offset */
	Elf64_Off e_shoff;                 /* Section header offset */
	Elf64_Word e_flags;                /* Processor-specific flags */
	Elf64_Half e_ehsize;               /* ELF header size */
	Elf64_Half e_phentsize;            /* Size of program header entry */
	Elf64_Half e_phnum;                /* Number of program header entries */
	Elf64_Half e_shentsize;            /* Size of section header entry */
	Elf64_Half e_shnum;                /* Number of section header entries */
	Elf64_Half e_shstrndx;             /* Section name string table index */
} Elf64_Ehdr;

typedef struct {
	Elf64_Word sh_name;       /* Section name */
	Elf64_Word sh_type;       /* Section type */
	Elf64_Xword sh_flags;     /* Section attributes */
	Elf64_Addr sh_addr;       /* Virtual address in memory */
	Elf64_Off sh_offset;      /* Offset in file */
	Elf64_Xword sh_size;      /* Size of section */
	Elf64_Word sh_link;       /* Link to other section */
	Elf64_Word sh_info;       /* Miscellaneous information */
	Elf64_Xword sh_addralign; /* Address alignment boundary */
	Elf64_Xword sh_entsize;   /* Size of entries, if section has table */
} Elf64_Shdr;

typedef struct {
	Elf64_Word st_name;     /* Symbol name */
	unsigned char st_info;  /* Type and Binding attributes */
	unsigned char st_other; /* Reserved */
	Elf64_Half st_shndx;    /* Section table index */
	Elf64_Addr st_value;    /* Symbol value */
	Elf64_Xword st_size;    /* Size of object (e.g., common) */
} Elf64_Sym;

typedef struct {
	Elf64_Word p_type;    /* Type of segment */
	Elf64_Word p_flags;   /* Segment attributes */
	Elf64_Off p_offset;   /* Offset in file */
	Elf64_Addr p_vaddr;   /* Virtual address in memory */
	Elf64_Addr p_paddr;   /* Reserved */
	Elf64_Xword p_filesz; /* Size of segment in file */
	Elf64_Xword p_memsz;  /* Size of segment in memory */
	Elf64_Xword p_align;  /* Alignment of segment */
} Elf64_Phdr;

enum Elf64_PhdrType { PT_NULL = 0, PT_LOAD = 1, PT_DYNAMIC = 2, PT_INTERP = 3, PT_NOTE = 4, PT_SHLIB = 5, PT_PHDR = 6 };

struct Elf64Types {
	typedef uint64_t addr_t;
	typedef Elf64_Ehdr Elf_Ehdr;
	typedef Elf64_Phdr Elf_Phdr;
	typedef Elf64_Shdr Elf_Shdr;
	typedef Elf64_Sym Elf_Sym;
	static constexpr unsigned PT_LOAD = Elf64_PhdrType::PT_LOAD;
};

typedef GenericElfLoader<Elf64Types> ELFLoader;

} /* namespace cheriv9::rv64 */
