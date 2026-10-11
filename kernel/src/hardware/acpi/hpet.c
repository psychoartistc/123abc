#include <drivers/api.h>
#include <hardware/acpi/parse.h>
#include <stddef.h>
#include <utils/logging.h>

#define HPET_GAS_SYSTEM_MEMORY 0
#define HPET_GAS_SYSTEM_IO 1

typedef struct {
  acpi_header_t header;
  uint32_t event_timer_block_id;
  acpi_gas_t base_address;
  uint8_t hpet_number;
  uint16_t minimum_tick;
  uint8_t page_protection;
} __PACKED hpet_table_t;

static int hpet_parse(acpi_header_t *hdr) {
  hpet_table_t *hpet = (hpet_table_t *)hdr;

  if (hpet->base_address.address_space_id != HPET_GAS_SYSTEM_MEMORY) {
    pr_warn("[hpet] hpet is not memory mapped");
    return -1;
  }

  acpi_parse_res_t *res = acpi_get_parse_res();
  res->hpet_phys = hpet->base_address.address;

  pr_info("[hpet] hpet timer address at 0x%p", res->hpet_phys);
  return 0;
}

static int hpet_parser_entry() {
  acpi_add_parser(
      (acpi_parser_t){.tbl_signature = "HPET", .on_parse = hpet_parse});
  return 0;
}

REGISTER_DRIVER(madt_parser, "hpet acpi table parser",
                "parses the hpet acpi table", "1.0.0", DRVLICENSE_DEFAULT,
                DRV_INIT_ORDER_ACPI_PARSER, hpet_parser_entry, NULL);
