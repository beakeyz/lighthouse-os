#include "parser.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/multiboot.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include <libk/stddef.h>
#include <libk/string.h>
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actypes.h"
#include "tables.h"

static acpi_parser_rsdp_discovery_method_t create_rsdp_method_state(enum acpi_rsdp_method method) {
  const char* name;
  switch (method) {
    case MULTIBOOT_NEW:
      name = "Multiboot XSDP header";
      break;
    case MULTIBOOT_OLD:
      name = "Multiboot RSDP header";
      break;
    case BIOS_POKE:
      name = "BIOS memory poking";
      break;
    case RECLAIM_POKE:
      name = "Reclaimable memory poking";
      break;
    case NONE:
      name = "None";
      break;
  }

  return (acpi_parser_rsdp_discovery_method_t) {
    .m_method = method,
    .m_name = name
  };
}

int init_acpi_parser_early(acpi_parser_t* parser)
{
  if (!parser)
    return -1;

  if (find_rsdp(parser) == NULL)
    return -2;

  if (acpi_parser_get_rsdp_method(parser) == NONE)
    return -3;

  return 0;
}

/*!
 * @brief: 'Late' init of the native ACPI parser
 *
 * At this point acpica is initialized, so if it's enabled, we can use it's methods
 *
 * TODO: use acpica
 */
ErrorOrPtr init_acpi_parser(acpi_parser_t* parser) 
{
  println("Starting ACPI parser");

  /* I stil hate init_list lmaoo */
  parser->m_tables = init_list();

  /*
   * Use the r/xsdp in order to find all the tables, 
   * cache their addresses and premap their headers
   */
  parser_init_tables(parser);

  //hw_reduced = ((parser->m_fadt->flags >> 20) & 1);

  println("Started");
  return Success(0);
}

/*!
 * @brief Find the ACPI tables, map them and reserve their memory
 *
 * TODO: should we create a nice system-wide resource entry for every table?
 */
void parser_init_tables(acpi_parser_t* parser) 
{
  ACPI_STATUS stat;
  ACPI_TABLE_HEADER* hdr;
  uint32_t tbl_count;
  acpi_tbl_rsdt_t* rsdt;

  if (!parser || !parser->m_rsdp_table)
    return;

  rsdt = (void*)Must(__kmem_kernel_alloc(parser->m_rsdt_phys, sizeof(acpi_tbl_rsdt_t), NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));
  rsdt = (void*)Must(__kmem_kernel_alloc(parser->m_rsdt_phys, rsdt->Header.Length, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  if (parser->m_is_xsdp)
    tbl_count = (rsdt->Header.Length - sizeof(rsdt->Header)) / sizeof(uint64_t);
  else
    tbl_count = (rsdt->Header.Length - sizeof(rsdt->Header)) / sizeof(uint32_t);
  
  for (uint64_t i = 0; i < tbl_count; i++) {
    stat = AcpiGetTableByIndex(i, &hdr);

    if (stat == AE_ERROR)
      break;

    printf("Found ACPI table: %-4.4s\n", hdr->Signature);

    list_append(parser->m_tables, hdr);
  }
}

/*!
 * @brief Try a few funky methods to find the RSDP / XSDP
 *
 * @parser: the parser object to store our findings
 */
void* find_rsdp(acpi_parser_t* parser) 
{
  // TODO: check other spots
  paddr_t rsdp_addr;

  parser->m_is_xsdp = false;
  parser->m_rsdp_table = nullptr;
  parser->m_rsdt_phys = NULL;
  parser->m_rsdp_phys = NULL;
  parser->m_xsdp_phys = NULL;
  parser->m_rsdp_discovery_method = create_rsdp_method_state(NONE);

  // check multiboot header
  struct multiboot_tag_new_acpi* new_ptr = g_system_info.xsdp;

  if (new_ptr) {
    /* FIXME: check if ->rsdp is a virtual or physical address? */
    rsdp_addr = *(uintptr_t*)new_ptr->rsdp;

    printf("Multiboot has xsdp: 0x%llx\n", rsdp_addr);

    parser->m_is_xsdp = true;
    parser->m_xsdp_phys = rsdp_addr;
    parser->m_rsdp_table = (void*)Must(__kmem_kernel_alloc(rsdp_addr, sizeof(acpi_tbl_rsdp_t), NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));
    parser->m_rsdt_phys = parser->m_rsdp_table->XsdtPhysicalAddress;
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_NEW);

    printf("Mapped xsdp to 0x%llx", (vaddr_t)parser->m_rsdp_table);
    return parser->m_rsdp_table;
  }

  struct multiboot_tag_old_acpi* old_ptr = g_system_info.rsdp;

  if (old_ptr) {
    rsdp_addr = *(uintptr_t*)old_ptr->rsdp;

    printf("Multiboot has rsdp: 0x%llx\n", rsdp_addr);

    parser->m_rsdp_phys = rsdp_addr;
    parser->m_rsdp_table = (void*)Must(__kmem_kernel_alloc(rsdp_addr, sizeof(acpi_tbl_rsdp_t), NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));
    parser->m_rsdt_phys = parser->m_rsdp_table->RsdtPhysicalAddress;
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_OLD);
    return parser->m_rsdp_table;
  }

  kernel_panic("TODO: check ACPI root table scanning methods!");

  // check bios mem
  uintptr_t bios_start_addr = 0xe0000;
  size_t bios_mem_size = ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE);

  if (IsError(kmem_ensure_mapped(nullptr, bios_start_addr, bios_mem_size)))
    bios_start_addr = Must(__kmem_kernel_alloc(bios_start_addr, bios_mem_size, NULL, NULL));

  for (uintptr_t i = bios_start_addr; i < bios_start_addr + bios_mem_size; i+=16) {
    acpi_tbl_rsdp_t* potential = (void *)i;
    if (memcmp(RSDP_SIGNATURE, potential, strlen(RSDP_SIGNATURE))) {
      parser->m_rsdp_discovery_method = create_rsdp_method_state(RECLAIM_POKE);

      parser->m_rsdp_table = potential;

      if (potential->Revision >= 2 && potential->XsdtPhysicalAddress)
        parser->m_is_xsdp = true;

      return potential;
    }
  }

  const list_t* phys_ranges = kmem_get_phys_ranges_list();

  FOREACH(i, phys_ranges) {

    phys_mem_range_t *range = i->data;

    if (range->type != PMRT_ACPI_NVS && range->type != PMRT_ACPI_RECLAIM)
      continue;

    uintptr_t start = kmem_ensure_high_mapping(range->start);
    uintptr_t length = range->length;

    for (uintptr_t i = start; i < (start + length); i += 16) {
      acpi_tbl_rsdp_t* potential = (void *)i;

      if (memcmp(RSDP_SIGNATURE, potential, strlen(RSDP_SIGNATURE))) {
        parser->m_rsdp_discovery_method = create_rsdp_method_state(RECLAIM_POKE);
        parser->m_rsdp_table = potential;

        if (potential->Revision >= 2 && potential->XsdtPhysicalAddress)
          parser->m_is_xsdp = true;

        return potential;
      }
    }
  }

  parser->m_rsdp_discovery_method = create_rsdp_method_state(NONE);
  return nullptr;
}

/*!
 * @brief Find an ACPI table at a certain index
 *
 * Some ACPI tables (like AML tables) are present more than once. We need to be able to 
 * find these tables as well
 */
void* acpi_parser_find_table_idx(acpi_parser_t *parser, const char* sig, size_t index, size_t table_size) 
{
  ACPI_STATUS stat;
  ACPI_TABLE_HEADER* hdr;

  stat = AcpiGetTable((ACPI_STRING)sig, index, &hdr);

  if (stat == AE_ERROR)
    return nullptr;

  return hdr;
}

/*!
 * @brief Finds the first table that matches the signature
 *
 * This should pretty much only be used for tables where there is a 
 * guarantee that there is only one present on the system
 */
void* acpi_parser_find_table(acpi_parser_t *parser, const char* sig, size_t table_size) 
{
  return (void*)acpi_parser_find_table_idx(parser, sig, 0, table_size);
}

const int parser_get_acpi_tables(acpi_parser_t* parser, char* names) 
{
  FOREACH(i, parser->m_tables) {
    acpi_std_hdr_t* header = i->data;

    char sig[5];
    memcpy(&sig, header->Signature, 4);
    sig[4] = '\0';

    concat(names, sig, names);

    if (i->next)
      concat(names, ", ", names);
  }

  return 0;
}

void print_tables(acpi_parser_t* parser) {

  const char tables[parser->m_tables->m_length * 6];

  parser_get_acpi_tables(parser, (char*)tables);
  println("Tables");
  println(tables);
}

ACPI_STATUS acpi_eval_int(acpi_handle_t handle, ACPI_STRING string, ACPI_OBJECT_LIST* args, size_t* ret)
{
  ACPI_STATUS status;
  ACPI_OBJECT obj;
  ACPI_BUFFER buf;

  buf.Length = sizeof(obj);
  buf.Pointer = &obj;

  status = AcpiEvaluateObject(handle, string, args, &buf);

  if (ACPI_FAILURE(status))
    return status;

  if (obj.Type != ACPI_TYPE_INTEGER)
    return AE_BAD_DATA;

  *ret = obj.Integer.Value;
  return AE_OK;
}
