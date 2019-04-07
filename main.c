#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "loader.h"

int main(int argc, char *argv[])
{
	size_t i;
	s_binary *bin = NULL;
	s_section *section;
	s_symbol *symbol;
	char *fname, *section_name;
	int section_flag = 0, c;

	
	if (2 > argc) {
		printf("Usage: %s -b <binary> [-s <section name>]\n", argv[0]);
		return 1;
	}

	while ((c = getopt(argc, argv, "b:s:")) != -1) {
		switch (c) {
		case 's':
			section_flag = 1;
			section_name = optarg;
			break;
		case 'b':
			fname = optarg;
		default:
			break;
		}
	}

	if (load_binary(fname, &bin) < 0) {
		return 1;
	}

	if (0 != section_flag) {
		printf("HexDump of section %s\n", section_name);
		section = bin->get_section_by_name(bin, section_name);

		if (NULL == section) {
			return -1;
		}

		printf("0x00000000 ");
		for (i = 0; i < section->size; i++) {
			if (i % 16 == 0 && i != 0) {
				printf(" \n0x%08zX ", i);
			}
			printf("%02X ", section->bytes[i]);
		}
		printf(" \n");
	} else {
	
		printf("Loaded binary '%s' %s/%s (%u bits) entry@0x%016jx\n",
			   bin->filename, bin->type_str, bin->arch_str, bin->bits, bin->entry);

		for (i = 0; i < bin->nbr_sections; i++) {
			section = bin->sections[i];
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

	}
	unload_binary(&bin);
    return 0;
}
