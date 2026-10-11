#ifndef ACPI_PARSE_H_
#define ACPI_PARSE_H_

/*
 * hardware/acpi/acpi_parse.h
 * acpi tables parser */

#include <hardware/acpi/common.h>
#include <hardware/ioapic.h>
#include <mm/kern/slub.h>
#include <stddef.h>
#include <utils/int.h>

#define LAPIC_DEFAULT_PHYS 0xFEE00000
#define IOAPIC_DEFAULT_PHYS 0xFEC00000
#define HPET_DEFAULT_PHYS 0xFED00000

typedef struct {
  ioapic_t *data;
  size_t count;
  size_t capacity;
} acpi_ioapics_t;

typedef struct {
  acpi_ioapics_t ioapics;
  uptr lapic_phys;
  uptr hpet_phys;
  uptr ioapic_gsi_base;
} acpi_parse_res_t;

typedef int (*acpi_parse_fn_t)(acpi_header_t *hdt);

typedef struct {
  char tbl_signature[5];
  acpi_parse_fn_t on_parse;
} acpi_parser_t;

void acpi_add_parser(acpi_parser_t parser);

void acpi_ioapics_init(acpi_ioapics_t *ioapics);
void acpi_ioapics_push(acpi_ioapics_t *ioapics, ioapic_t record);
void acpi_ioapics_free(acpi_ioapics_t *ioapics);

// returns a pointer because the parsers might edit this
acpi_parse_res_t *acpi_get_parse_res();

void acpi_tables_parse();

#endif
