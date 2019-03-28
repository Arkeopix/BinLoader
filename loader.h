#ifndef    LOADER_H
#define    LOADER_H

#include <stdint.h>
#include <stdbool.h>

/* Begin s_symbol struct definition */

#define SYMTAB_UPPER_BOUND(BFD_H, DYN) \
	bfd_get ## DYN ## symtab_upper_bound(BFD_H)

#define SYMTAB_CANONICALIZE(BFD_H, SYMTAB, DYN) \
	bfd_canonicalize ## DYN ## symtab(BFD_H, SYMTAB)

typedef enum e_symbol_type { 
	SYM_TYPE_UKN = 0, 
	SYM_TYPE_FUNC = 1
} e_symbol_type;

typedef struct s_symbol {
	e_symbol_type type;
	char *name;
	uint64_t address;
} s_symbol;

s_symbol *new_symbol(e_symbol_type type, const char *name, uint64_t address);

/* End s_symbol struct definition */

/* Begin s_section struct definition */
typedef enum e_section_type { 
	SEC_TYPE_NONE = 0, 
	SEC_TYPE_CODE = 1,
	SEC_TYPE_DATA = 2
} e_section_type ;

typedef struct s_section {
	bool (*contains)(struct s_section* self, uint64_t address);

	char *name;
	e_section_type type;
	uint64_t vma;
	uint64_t size;
	uint8_t *bytes;
} s_section;

s_section *new_section(e_section_type type, uint64_t size, uint8_t *bytes, uint64_t vma, const char *secname);

/* End s_section struct definition */

/* Begin s_binary struct definition */
typedef enum e_binary_type {
	BIN_TYPE_AUTO = 0,
	BIN_TYPE_ELF = 1,
	BIN_TYPE_PE = 2
} e_binary_type;

typedef enum e_arch_type {
	ARCH_NONE = 0,
	ARCH_X86 = 1
} e_arch_type;

typedef struct s_binary {
	struct s_section *(*get_section_by_name)(struct s_binary *self, char *name);

	char *filename;
	e_binary_type type;
	char *type_str;
	e_arch_type arch;
	const char *arch_str;
	uint32_t bits;
	uint64_t entry;
	s_section **sections;
	uint32_t nbr_sections;
	s_symbol **symbols;
	uint32_t nbr_symbols;
} s_binary;

s_binary *new_binary(e_binary_type type, e_arch_type arch, uint32_t bits, uint64_t entry);

/* End s_binary struct definition */

int load_binary(char *fname, s_binary **bin);
void unload_binary(s_binary **bin);


#endif    /* LOADER_H */
