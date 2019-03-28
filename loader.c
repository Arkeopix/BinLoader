#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <bfd.h>
#include "loader.h"

static bfd *open_bfd(char *fname) 
{
	static int bfd_inited = FALSE;
	bfd *abfd = NULL;
	int status;
	
	/* We don't need to init bfd each time */
	if (FALSE == bfd_inited) {
		bfd_init();
		bfd_inited = TRUE;
	}

	/* We open the binary file fname and skip the second parameter so bfd can infer it for us */
	abfd = bfd_openr(fname, NULL);
	if (NULL == abfd) {
		fprintf(stderr, "Could not open binary %s: %s\n", fname, bfd_errmsg(bfd_get_error()));
		goto clean_exit;		
	}

	/* We check that the open'd binary is indeed an object file (executable, relocatable object, shared library) */
	status = bfd_check_format(abfd, bfd_object);
	if (FALSE == status) {
		fprintf(stderr, "File %s does not look like an object file: %s\n", fname, bfd_errmsg(bfd_get_error()));
		abfd = NULL;
		goto clean_exit;		
	}

	/* We get the type of object (coff, elf, etc.) */
	status = bfd_get_flavour(abfd);
	if (bfd_target_unknown_flavour == status) {
		fprintf(stderr, "Unrecognized format for binary %s: %s\n", fname, bfd_errmsg(bfd_get_error()));
		abfd = NULL;
	}
	
clean_exit:
	return abfd;
}

static int load_sections_bfd(bfd *abfd, s_binary **bin) {
	int bfd_flags, i, ret;
	uint64_t vma, size;
	const char *secname;
	asection *bfd_sec;
	e_section_type sec_type;
	uint8_t *bytes;

	(*bin)->sections = malloc(sizeof(s_section *));
	if (NULL == (*bin)->sections) {
		fprintf(stderr, "Could not allocate memory for section\n");
		goto fail;
	}

	for (bfd_sec = abfd->sections, i = 1; bfd_sec; bfd_sec = bfd_sec->next) {
		bfd_flags = bfd_get_section_flags(abfd, bfd_sec);

		sec_type = SEC_TYPE_NONE;
		if (bfd_flags & SEC_CODE) {
			sec_type = SEC_TYPE_CODE;
		} else if (bfd_flags & SEC_DATA) {
			sec_type = SEC_TYPE_DATA;
		} else {
			continue;
		}

		vma = bfd_section_vma(abfd, bfd_sec);
		size = bfd_section_size(abfd, bfd_sec);
		secname = bfd_section_name(abfd, bfd_sec);

		if (NULL == secname) {
			secname = "<unnamed>";
		}

		/* Allocate memory with realloc in order to store pointer to section */
		(*bin)->sections = realloc((*bin)->sections, i * sizeof(s_symbol *));
		bytes = malloc(size * sizeof(uint8_t));
		if (NULL == bytes) {
			fprintf(stderr, "Could not allocate memory for section bytes\n");
			goto fail;
		}
		ret = bfd_get_section_contents(abfd, bfd_sec, bytes, 0, size);
		if (0 > ret) {
			fprintf(stderr, "Could not get section %s content: %s\n", secname, bfd_errmsg(bfd_get_error()));
			goto fail;
		}
		(*bin)->sections[i - 1] = new_section(sec_type, size, bytes, vma, secname);
		//memcpy(secname, (*bin)->sections[i - 1]->name, strlen(secname))
		(*bin)->nbr_sections += 1;
		i++;
	}

	ret = 0;
	goto clean_exit;
fail:
	ret = -1;
clean_exit:
	return ret;
}

static int load_symbols_bfd(bfd *abfd, s_binary **bin, uint8_t dyn)
{
	int ret = 0;
	int64_t size_in_bytes, symbol_number;
	asymbol **bfd_symtab;

	bfd_symtab = NULL;

	/* Get size of symbol table */
	if (0 == dyn) {
		size_in_bytes = SYMTAB_UPPER_BOUND(abfd, _);
	} else {
		size_in_bytes = SYMTAB_UPPER_BOUND(abfd, _dynamic_);
	}
	if (0 > size_in_bytes) {
		fprintf(stderr, "Failed to read symtab: %s\n", bfd_errmsg(bfd_get_error()));
		goto fail;
	}

	/* Allocate memory for symbol table */
	bfd_symtab = malloc(size_in_bytes);
	if (NULL == bfd_symtab) {
		fprintf(stderr, "Could not allocate memory for symtab\n");
		goto fail;
	}

	/* Load symbol into bfd_symtab and returns tne total number of read symbols */
	if (0 == dyn) {
		symbol_number = SYMTAB_CANONICALIZE(abfd, bfd_symtab, _);
	} else {
		symbol_number = SYMTAB_CANONICALIZE(abfd, bfd_symtab, _dynamic_);
	}
	if (0 > symbol_number) {
		fprintf(stderr, "Failed to read symtab: %s\n", bfd_errmsg(bfd_get_error()));
		goto fail;
	}

	(*bin)->symbols = malloc(sizeof(s_symbol *));
	if (NULL == (*bin)->symbols) {
		fprintf(stderr, "Could not allocate memory for symtab\n");
		goto fail;
	}
	for (int i = 0, j = 1; i < symbol_number; i++) {
		if (bfd_symtab[i]->flags & BSF_FUNCTION) {
			/* Allocate memory for each pointer */
			(*bin)->symbols = realloc((*bin)->symbols, j * sizeof(s_symbol *));
			(*bin)->symbols[j - 1] = new_symbol(SYM_TYPE_FUNC, bfd_symtab[i]->name, bfd_asymbol_value(bfd_symtab[i]));
			(*bin)->nbr_symbols += 1;
			j++;
		}
	}
	
	ret = 0;
	goto clean_exit;
fail:
	ret = -1;
clean_exit:
	if (NULL != bfd_symtab) {
		/* if we free, nothing will stand when we exit the fucntion */
		/* We will have to create copies of all strings and values */
		free(bfd_symtab);
	}
	return ret;
}

static int load_binary_bfd(char *fname, s_binary **bin) 
{
	int ret = 0;
	bfd *abfd = NULL;
	const bfd_arch_info_type *bfd_info = NULL;
	int type, arch, bits;

	/* Open fname */
	abfd = open_bfd(fname);
	if (NULL == abfd) {
		goto fail;
	}

	/* Start filling up bin struct */

	switch (abfd->xvec->flavour) {
	case bfd_target_elf_flavour:
		type = BIN_TYPE_ELF;
		break;
	case bfd_target_coff_flavour:
		type = BIN_TYPE_PE;
		break;
	case bfd_target_unknown_flavour:
	default:
		fprintf(stderr, "Unsuported binary type %s\n", abfd->xvec->name);
		goto fail;
	}

	bfd_info = bfd_get_arch_info(abfd);

	switch (bfd_info->mach) {
	case bfd_mach_i386_i386:
		arch = ARCH_X86;
		bits = 32;
		break;
	case bfd_mach_x86_64:
		arch = ARCH_X86;
		bits = 64;
		break;
	default:
		fprintf(stderr, "Unsuported architecture %s\n", bfd_info->printable_name);
		goto fail;
	}

	*bin = new_binary(type, arch, bits, bfd_get_start_address(abfd));
	(*bin)->filename = fname;
	(*bin)->type_str = abfd->xvec->name;
	(*bin)->arch_str = bfd_info->printable_name;
	
	/* Symbols */
	load_symbols_bfd(abfd, bin, 0);
	load_symbols_bfd(abfd, bin, 1);

	/* sections */
	ret = load_sections_bfd(abfd, bin);
	if (0 > ret) {
		goto clean_exit;
	}

	ret = 0;
	goto clean_exit;
fail:
	ret = -1;
	
clean_exit:
	if (NULL != abfd) {
		bfd_close(abfd);
	}
	return ret;
}

s_symbol *new_symbol(e_symbol_type type, const char *name, uint64_t address) 
{
	s_symbol*symbol = NULL;
	size_t len;
	
	symbol = malloc(sizeof(s_symbol));
	if (NULL == symbol) {
		fprintf(stderr, "Could not allocate memory for symbol\n");
		goto clean_exit;
	}

	symbol->address = address;
	symbol->type = type;
	len = strlen(name);
	symbol->name = malloc((len * sizeof(*name)) + 1);
	if (NULL == symbol->name) {
		fprintf(stderr, "Could not allocate memory for symbol\n");
		goto clean_exit;
	}
	symbol->name[len] = 0;
	symbol->name = memcpy(symbol->name, name, strlen(name));

clean_exit:
	return symbol;
}

static bool _contains(s_section *self, uint64_t address) 
{
	return (address >= self->vma) && (address - self->vma <= self->size);
}

s_section *new_section(e_section_type type, uint64_t size, uint8_t *bytes, uint64_t vma, const char *secname) 
{
	s_section *section = NULL;
	size_t len;
	
	section = malloc(sizeof(s_section));
	if (NULL == section) {
		fprintf(stderr, "Could not allocate memory for section\n");
		goto clean_exit;
	}

	section->type = type;
	section->vma = vma;
	section->size = size;
	section->bytes = bytes;

	len = strlen(secname);
	section->name = malloc((len * sizeof(*secname)) + 1);
	if (NULL == section->name) {
		fprintf(stderr, "Could not allocate memory for section\n");
		goto clean_exit;
	}
	memcpy(section->name, secname, strlen(secname));
	section->name[len] = 0;
	section->contains = &_contains;
	
clean_exit:
	return section;
}

s_section *_get_section_by_name(s_binary *self, char *name) 
{
	self = self;
	name = name;
	return NULL;
}

s_binary *new_binary(e_binary_type type, e_arch_type arch, uint32_t bits, uint64_t entry) 
{
	s_binary *binary = NULL;

	binary = malloc(sizeof(s_binary));
	if (NULL == binary) {
		fprintf(stderr, "Could not allocate memory for bynary\n");
		goto clean_exit;
	}

	binary->type = type;
	binary->arch = arch;
	binary->bits = bits;
	binary->entry = entry;
	binary->nbr_sections = 0;
	binary->nbr_symbols = 0;
	
	binary->get_section_by_name = &_get_section_by_name;
clean_exit:
	return binary;
}

int load_binary(char *fname, s_binary **bin) 
{
	return load_binary_bfd(fname, bin);
}

void unload_binary(s_binary **bin) 
{
	size_t i;
	s_section *section;
	s_symbol *symbol;

	for (i = 0; i < (*bin)->nbr_sections; i++) {
		section = (*bin)->sections[i];
		if (section->bytes) {
			free(section->bytes);
		}
		if (section->name) {
			free(section->name);
		}
	}
	for (i = 0; i < (*bin)->nbr_symbols; i++) {
		symbol = (*bin)->symbols[i];
		if (symbol->name) {
			free(symbol->name);
		}
	}
	free((*bin)->symbols);
	free((*bin)->sections);
	free((*bin));
}
