#ifndef ACPI_COMMON_H_
#define ACPI_COMMON_H_

#include <utils/attributes.h>
#include <utils/int.h>

/*
 * hardware/acpi/common.h
 * common acpi structs*/

typedef struct {
  char signature[4];
  u32 length;
  u8 revision;
  u8 checksum;
  char oem_id[6];
  char oem_table_id[8];
  u32 oem_revision;
  u32 creator_id;
  u32 creator_revision;
} __PACKED acpi_header_t;

typedef struct {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t extended_checksum;
  uint8_t reserved[3];
} __PACKED rsdp_t;

typedef struct {
  acpi_header_t header;
} __PACKED rsdt_t;

typedef struct {
  acpi_header_t header;
  u64 tables[];
} __PACKED xsdt_t;

typedef struct {
  uint8_t address_space_id;
  uint8_t register_bit_width;
  uint8_t register_bit_offset;
  uint8_t access_size;
  uint64_t address;
} __PACKED acpi_gas_t;

#endif
