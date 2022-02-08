#pragma once

#include <boost/iostreams/device/mapped_file.hpp>
#include <exception>
#include <cstdint>
#include <vector>

#include "load_if.h"

template <typename T>
struct GenericElfLoader {
	typedef typename T::addr_t addr_t;
	typedef typename T::Elf_Ehdr Elf_Ehdr;
	typedef typename T::Elf_Phdr Elf_Phdr;
	typedef typename T::Elf_Shdr Elf_Shdr;
	typedef typename T::Elf_Sym Elf_Sym;
	static_assert(sizeof(addr_t) == sizeof(Elf_Ehdr::e_entry), "architecture mismatch");

	const char *filename;
	boost::iostreams::mapped_file_source elf;
	const Elf_Ehdr *hdr;

	struct load_executable_exception : public std::exception {
		const char * what () const throw () {
			return "Tried loading invalid elf layout";
		}
	};

	GenericElfLoader(const char *filename) : filename(filename), elf(filename) {
		assert(elf.is_open() && "file not open");

		hdr = reinterpret_cast<const Elf_Ehdr *>(elf.data());
	}

	std::vector<const Elf_Phdr *> get_load_sections() {
		std::vector<const Elf_Phdr *> sections;

		for (int i = 0; i < hdr->e_phnum; ++i) {
			const Elf_Phdr *p =
			    reinterpret_cast<const typename T::Elf_Phdr *>(elf.data() + hdr->e_phoff + hdr->e_phentsize * i);

			if (p->p_type != T::PT_LOAD)
				continue;

			if ((p->p_filesz == 0) && (p->p_memsz == 0))
				continue;

			//If p_memsz is greater than p_filesz, the extra bytes are NOBITS.
			// -> still, the memory needs to be zero initialized in this case!
//			if (p->p_memsz > p->p_filesz)
//				continue;

			sections.push_back(p);
		}

		return sections;
	}

	std::ostream& print_phdr(std::ostream& os, const Elf_Phdr& h, unsigned tabs = 0)
	{
		std::string tab(tabs, '\t');
		os << tab << "p_type "  << h.p_type   << std::endl;
		os << tab << "p_offset "<< h.p_offset << std::endl;
		os << tab << "p_vaddr " << h.p_vaddr  << std::endl;
		os << tab << "p_paddr " << h.p_paddr  << std::endl;
		os << tab << "p_filesz "<< h.p_filesz << std::endl;
		os << tab << "p_memsz " << h.p_memsz  << std::endl;
		os << tab << "p_flags " << h.p_flags  << std::endl;
		os << tab << "p_align " << h.p_align  << std::endl;
	    return os;
	}

	void load_executable_image(load_if &load_if, addr_t size, addr_t offset, bool use_vaddr = true) {
		for (auto p : get_load_sections()) {
			auto addr = p->p_paddr;
			if (use_vaddr)
				addr = p->p_vaddr;

			// If the modeled virtual platform separates Flash and DRAM
			// (like the hifive-vp does) we call load_executable_image
			// once for each memory segment (i.e. once for the Flash and
			// once for the DRAM). As such, we must skip all ELF regions
			// which are not covered by the passed memory segment.
			if (!(addr >= offset && addr < (offset + size)))
				continue;

			if (addr < offset) {
				std::cerr << "[elf_loader] ";
				std::cerr << "Offset overlaps into section:" << std::endl;
				std::cerr << "\t0x" << std::hex << +addr << " < " << +offset << std::endl;
				print_phdr(std::cerr, *p, 2);
				throw load_executable_exception();
			}

			if (addr + p->p_memsz >= offset + size) {
				std::cerr << "[elf_loader] ";
				std::cerr << "Section does not fit in target memory" << std::endl;
				std::cerr << "\t0x" << std::hex << +addr << " + size 0x" << +p->p_memsz;
				std::cerr << " would overflow offset 0x" << +offset << " + size 0x" << +size << std::endl;
				print_phdr(std::cerr, *p, 2);
				throw load_executable_exception();
			}

			auto idx = addr - offset;
			const char *src = elf.data() + p->p_offset;
			auto to_copy = p->p_filesz;

			load_if.load_data(src, idx, to_copy);

			assert (p->p_memsz >= p->p_filesz);
			idx = idx + p->p_filesz;
			to_copy = p->p_memsz - p->p_filesz;

			load_if.load_zero(idx, to_copy);
		}
	}

	addr_t get_memory_end() {
		const Elf_Phdr *last =
		    reinterpret_cast<const Elf_Phdr *>(elf.data() + hdr->e_phoff + hdr->e_phentsize * (hdr->e_phnum - 1));

		return last->p_vaddr + last->p_memsz;
	}

	addr_t get_heap_addr() {
		// return first 8 byte aligned address after the memory image
		auto s = get_memory_end();
		return s + s % 8;
	}

	addr_t get_entrypoint() {
		return hdr->e_entry;
	}

	const char *get_section_string_table() {
		assert(hdr->e_shoff != 0 && "string table section not available");

		const Elf_Shdr *s =
		    reinterpret_cast<const Elf_Shdr *>(elf.data() + hdr->e_shoff + hdr->e_shentsize * hdr->e_shstrndx);
		const char *start = elf.data() + s->sh_offset;
		return start;
	}

	const char *get_symbol_string_table() {
		auto s = get_section(".strtab");
		return elf.data() + s->sh_offset;
	}

	const Elf_Sym *get_symbol(const char *symbol_name) {
		const Elf_Shdr *s = get_section(".symtab");
		const char *strings = get_symbol_string_table();

		assert(s->sh_size % sizeof(Elf_Sym) == 0);

		auto num_entries = s->sh_size / sizeof(typename T::Elf_Sym);
		for (unsigned i = 0; i < num_entries; ++i) {
			const Elf_Sym *p = reinterpret_cast<const Elf_Sym *>(elf.data() + s->sh_offset + i * sizeof(Elf_Sym));

			// std::cout << "check symbol: " << strings + p->st_name << std::endl;

			if (!strcmp(strings + p->st_name, symbol_name)) {
				return p;
			}
		}

		throw std::runtime_error("unable to find symbol in the symbol table " + std::string(symbol_name));
	}

	addr_t get_begin_signature_address() {
		auto p = get_symbol("begin_signature");
		return p->st_value;
	}

	addr_t get_end_signature_address() {
		auto p = get_symbol("end_signature");
		return p->st_value;
	}

    addr_t get_to_host_address() {
        auto p = get_symbol("tohost");
        return p->st_value;
    }

	std::vector<const Elf_Shdr *> get_sections(void) {
		if (hdr->e_shoff == 0) {
			throw std::runtime_error("unable to find section address, section table not available");
		}

		std::vector<const Elf_Shdr *> sections;
		for (unsigned i = 0; i < hdr->e_shnum; ++i) {
			const Elf_Shdr *s = reinterpret_cast<const Elf_Shdr *>(elf.data() + hdr->e_shoff + hdr->e_shentsize * i);
			sections.push_back(s);
		}

		return sections;
	}

	const Elf_Shdr *get_section(const char *section_name) {
		const char *strings = get_section_string_table();

		for (auto s : get_sections()) {
			if (!strcmp(strings + s->sh_name, section_name)) {
				return s;
			}
		}

		throw std::runtime_error("unable to find section address, section seems not available: " +
		                         std::string(section_name));
	}
};
