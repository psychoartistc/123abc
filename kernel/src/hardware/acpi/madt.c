#include <drivers/api.h>
#include <hardware/acpi/parse.h>
#include <hardware/lapic.h>
#include <hardware/pic8259.h>
#include <stddef.h>
#include <utils/logging.h>
#include <utils/panic.h>

#define MADT_IOAPIC_CODE 1

typedef struct {
  acpi_header_t header;
  uint32_t local_apic_address;
  uint32_t flags;
  uint8_t entries[];
} __PACKED madt_t;

typedef struct {
  uint8_t type;
  uint8_t length;
} __PACKED madt_entry_header_t;

typedef struct {
  madt_entry_header_t header;
  uint8_t io_apic_id;
  uint8_t reserved;
  uint32_t io_apic_address;
  uint32_t global_system_interrupt_base;
} __PACKED madt_ioapic_t;

static void madt_parse_ioapic(madt_ioapic_t *ioapic) {
  acpi_parse_res_t *res = acpi_get_parse_res();
  ioapic_t record = {.ioapic_phys = ioapic->io_apic_address,
                     .ioapic_gsi_base = ioapic->global_system_interrupt_base};
  ioapic_init(&record);

  acpi_ioapics_push(&res->ioapics, record);
}

static int madt_parse(acpi_header_t *header) {
  madt_t *madt = (madt_t *)header;
  acpi_parse_res_t *res = acpi_get_parse_res();

  res->lapic_phys = madt->local_apic_address;
  lapic_init();
  pr_info("[madt] lapic physical address at 0x%p", res->lapic_phys);

  uintptr_t madt_end = (uintptr_t)madt + madt->header.length;
  uintptr_t current_entry = (uintptr_t)madt->entries;

  while (current_entry < madt_end) {
    madt_entry_header_t *entry = (madt_entry_header_t *)current_entry;

    if (entry->type == MADT_IOAPIC_CODE)
      madt_parse_ioapic((madt_ioapic_t *)entry);

    if (entry->length == 0)
      break;

    current_entry += entry->length;
  }

  assert(res->ioapics.count > 0 && "[madt] no ioapics found, cannot proceed");

  pic8259_disable();
  pr_info("[madt] added %zu ioapic%s", res->ioapics.count,
          res->ioapics.count > 1 ? "s" : "");

  return 0;
}

static int madt_parser_entry() {
  acpi_add_parser(
      (acpi_parser_t){.tbl_signature = "APIC", .on_parse = madt_parse});

  return 0;
}

REGISTER_DRIVER(madt_parser, "madt acpi table parser",
                "parses the madt acpi table", "1.0.0", DRVLICENSE_DEFAULT,
                DRV_INIT_ORDER_ACPI_PARSER, madt_parser_entry, NULL);
