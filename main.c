#include <stdio.h>
#include <stdint.h>
#include "loader.h"

int main(int argc, char *argv[])
{
	size_t i;
	s_binary *bin = NULL;
	s_section *section;
	s_symbol *symbol;
	char *fname;

	if (2 > argc) {
		printf("Usage: %s <binary>\n", argv[0]);
		return 1;
	}

	fname = argv[1];

	if (load_binary(fname, &bin) < 0) {
		return 1;
	}

	printf("Loaded binary '%s' %s/%s (%u bits) entry@0x%016jx\n",
		   bin->filename, bin->type_str, bin->arch_str, bin->bits, bin->entry);

	for (i = 0; i < bin->nbr_sections; i++) {
		section = bin->sections[i];
		/* use of freed memory here, works but needs fixing */
		/* Need to copy every value in s_binary struct instead of storing pointers */
		printf(" 0x%016jx %-8ju %-20s %s\n",
			   section->vma, section->size, section->name, section->type == SEC_TYPE_CODE ? "CODE" : "DATA");
	}

	if (bin->nbr_symbols > 0) {
		printf("found symbol table\n");
		for (i = 0; i < bin->nbr_symbols; i++) {
			symbol = bin->symbols[i];
			printf(" %-40s 0x%016jx %s\n",
				   symbol->name, symbol->address, (symbol->type & SYM_TYPE_FUNC) ? "FUNC" : "");
		}
	}

	unload_binary(&bin);
    return 0;
}
