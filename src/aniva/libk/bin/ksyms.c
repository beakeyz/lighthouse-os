#include "ksyms.h"
#include "dev/core.h"
#include "dev/driver.h"
#include "mem/kmem_manager.h"
#include <libk/string.h>

/*
 * Kernel symbol manager
 *
 * The opperations that happen here are quite CPU heavy, since some require us to scan the entire
 * symbol table. We should implement some kind of caching system to make these lookups faster
 */

extern char* _start_ksyms[];
extern char* _end_ksyms[];

extern size_t __ksyms_count;
extern uint8_t __ksyms_table;

const char* get_ksym_name(uintptr_t address)
{
    uint8_t* i = &__ksyms_table;
    ksym_t* current_symbol = (ksym_t*)i;

    while (current_symbol->sym_len) {
        current_symbol = (ksym_t*)i;

        if (current_symbol->address == address)
            return current_symbol->name;

        i += current_symbol->sym_len;
    }

    return nullptr;
}

/*!
 * @brief: Grab the best symbol from a driver for a specific address
 */
static const char* __get_driver_sym(driver_t* driver, uintptr_t address)
{
    /* TODO: */
    return nullptr;
}

const char* get_best_ksym_name(uintptr_t address)
{
    uint8_t* i = &__ksyms_table;
    ksym_t* c_symbol = (ksym_t*)i;
    ksym_t* best_symbol = nullptr;
    size_t best_addr = 0;
    driver_t* driver;

    /* No kernel address, fuck off */
    if (address < KERNEL_MAP_BASE)
        return nullptr;

    /* Try to fetch a driver for this address */
    driver = get_driver_from_address(address);

    /* Found a driver check */
    if (driver)
        return __get_driver_sym(driver, address);

    while (c_symbol->sym_len) {
        c_symbol = (ksym_t*)i;

        /* Check if the address is inside this function */
        if (c_symbol->address > address)
            goto cycle_next;

        /* Check if this delta is better than the current best delta */
        if (c_symbol->address < best_addr)
            goto cycle_next;

        best_addr = c_symbol->address;
        best_symbol = c_symbol;

    cycle_next:
        i += c_symbol->sym_len;
    }

    if (!best_symbol)
        return nullptr;

    return best_symbol->name;
}

uintptr_t get_ksym_address(char* name)
{
    uint8_t* i = &__ksyms_table;
    ksym_t* current_symbol = (ksym_t*)i;

    if (!name || !*name)
        return NULL;

    while (current_symbol->sym_len) {
        current_symbol = (ksym_t*)i;

        if (memcmp(name, current_symbol->name, strlen(current_symbol->name)) && memcmp(name, current_symbol->name, strlen(name)))
            return current_symbol->address;

        i += current_symbol->sym_len;
    }

    return NULL;
}

size_t get_total_ksym_area_size()
{
    return (_end_ksyms - _start_ksyms);
}
