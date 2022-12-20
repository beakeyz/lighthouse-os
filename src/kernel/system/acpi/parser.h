#ifndef __LIGHT_ACPI_PARSER__ 
#define __LIGHT_ACPI_PARSER__

// TODO: fill structure
typedef struct {

} acpi_parser_t;

void init_acpi_parser();

// find rsdt and (if available) xsdt
void* find_rsdt();

// me want cool table
void find_table(const char* sig);

// TODO: find needed params
void start_aml_parsing();


#endif // !
