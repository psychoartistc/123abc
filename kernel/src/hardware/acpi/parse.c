#include <hardware/acpi/parse.h>
#include <limine_requests.h>
#include <mm/common.h>
#include <stdatomic.h>
#include <stddef.h>
#include <utils/branching.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include <utils/panic.h>

#define MAX_PARSERS 32

acpi_parser_t s_parsers[MAX_PARSERS] = {0};
acpi_parse_res_t s_parseRes = {0};
u64 s_parsersCount = 0;

void acpi_add_parser(acpi_parser_t parser) {
  s_parsers[atomic_fetch_add(&s_parsersCount, 1)] = parser;
}

// returns a pointer because the parsers might edit this
acpi_parse_res_t *acpi_get_parse_res() { return &s_parseRes; }

static void parse_tbl(acpi_header_t *tbl) {
  if (unlikely(!tbl))
    return;

  for (size_t j = 0; j < s_parsersCount; j++) {
    acpi_parser_t parser = s_parsers[j];
    if (strlcmp(tbl->signature, 4, parser.tbl_signature, 4) == 0) {
      int r = parser.on_parse(tbl);
      pr_info("\t%.4s: %s", tbl->signature, (r < 0) ? "ERR" : "OK");
      return;
    }
  }
  pr_info("\t%.4s: NOT FOUND", tbl->signature);
}

static void parse_rsdt(rsdp_t *rsdp) {
  rsdt_t *rsdt = (rsdt_t *)p2v(rsdp->rsdt_address);
  size_t entry_count =
      (rsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint32_t);
  uint32_t *tbls = (uint32_t *)((uintptr_t)rsdt + sizeof(acpi_header_t));

  pr_info("[acpi] rsdt parse results:");
  for (size_t i = 0; i < entry_count; i++) {
    acpi_header_t *tbl = (acpi_header_t *)p2v(tbls[i]);
    if (unlikely(!tbl))
      continue;

    parse_tbl(tbl);
  }
}

void acpi_ioapics_init(acpi_ioapics_t *ioapics) {
  if (unlikely(!ioapics))
    return;

  ioapics->count = 0;
  ioapics->capacity = 4;
  ioapics->data = (ioapic_t *)kmalloc(ioapics->capacity * sizeof(int));
  if (!ioapics->data)
    panic("[acpi] failed to create ioapics dynamic vector, ioapics cannot be "
          "stored, cannot proceed");
}

void acpi_ioapics_free(acpi_ioapics_t *ioapics) {
  if (unlikely(!ioapics))
    return;

  kfree(ioapics->data);
  ioapics->data = NULL;
  ioapics->count = 0;
  ioapics->capacity = 0;
}

void acpi_ioapics_push(acpi_ioapics_t *ioapics, ioapic_t record) {
  if (unlikely(!ioapics))
    return;

  if (ioapics->count == ioapics->capacity) {
    ioapics->capacity *= 2;
    ioapic_t *newData = (ioapic_t *)krealloc(
        ioapics->data, ioapics->capacity * sizeof(ioapic_t));
    if (!newData)
      panic("[acpi] failed to create ioapics dynamic vector, ioapics cannot be "
            "stored, cannot proceed");
    ioapics->data = newData;
  }

  ioapics->data[ioapics->count] = record;
  ioapics->count++;
}

static void parse_xsdt(rsdp_t *rsdp) {
  xsdt_t *xsdt = (xsdt_t *)p2v(rsdp->xsdt_address);

  size_t entry_count =
      (xsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint64_t);
  pr_info("[acpi] xsdt parse results:");
  for (size_t i = 0; i < entry_count; i++) {
    acpi_header_t *tbl = (acpi_header_t *)p2v(xsdt->tables[i]);
    if (unlikely(!tbl))
      continue;

    parse_tbl(tbl);
  }
}

void acpi_tables_parse() {
  if (!rsdp_request.response || !rsdp_request.response->address) {
    s_parseRes = (acpi_parse_res_t){.lapic_phys = LAPIC_DEFAULT_PHYS,
                                    .hpet_phys = HPET_DEFAULT_PHYS};
    acpi_ioapics_push(
        &s_parseRes.ioapics,
        (ioapic_t){.ioapic_phys = IOAPIC_DEFAULT_PHYS, .ioapic_gsi_base = 0x0});

    pr_warn("[acpi] no rsdp provided, falling back to default physical "
            "addresses for "
            "acpi elements\n"
            "[acpi] note that this can be extremely dangerous if these "
            "addresses mismatch");
    return;
  }

  acpi_ioapics_init(&s_parseRes.ioapics);

  rsdp_t *rsdp = (rsdp_t *)rsdp_request.response->address;
  bool is_xsdt = (rsdp->revision >= 2 && rsdp->xsdt_address != 0);
  pr_info("[acpi] parsing %s", (is_xsdt) ? "xsdt" : "rsdt");
  if (is_xsdt)
    parse_xsdt(rsdp);
  else
    parse_rsdt(rsdp);
}
