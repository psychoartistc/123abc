#ifndef DRIVERS_H_
#define DRIVERS_H_

/*
 * drivers/api.h
 * driver registering api
 * */

#include <stdbool.h>
#include <utils/int.h>

#define DRVLICENSE_GPL "GPL"
#define DRVLICENSE_GPL2 "GPL2"
#define DRVLICENSE_GPL3 "GPL3"

#define DRVLICENSE_BSD0 "BSD0"
#define DRVLICENSE_BSD2 "BSD2"
#define DRVLICENSE_BSD3 "BSD3"
#define DRVLICENSE_BSD4 "BSD4"

#define DRVLICENSE_APACHE "Apache"

#define DRVLICENSE_MIT "MIT"

#define DRVLICENSE_MPL "MPL"

#define DRVLICENSE_CC0 "CC0"

#define DRVLICENSE_PROPRIETARY "Proprietary"

#define DRVLICENSE_DEFAULT DRVLICENSE_GPL3

#define DRV_INIT_ORDER_URGENT 0
#define DRV_INIT_ORDER_AFTER_MEM 1
#define DRV_INIT_ORDER_NORMAL 2
#define DRV_INIT_ORDER_CLOCK_SOURCE 3
#define DRV_INIT_ORDER_ACPI_PARSER 4

typedef int (*drv_init_fn_t)(void);
typedef void (*drv_exit_fn_t)(void);

typedef struct driver {
  const char *name, *description, *version, *license;
  drv_init_fn_t init;
  drv_exit_fn_t exit;

  u8 init_order;
  bool initialized;
} driver_t;

void drivers_initialize_some(int level);

#define DRIVER_SECTION __attribute__((section(".drivers"), used, aligned(8)))

#define REGISTER_DRIVER(symbol, driver_name, driver_description,               \
                        driver_version, driver_license, driver_order,          \
                        init_function, exit_function)                          \
  static struct driver symbol DRIVER_SECTION = {.name = driver_name,           \
                                                .description =                 \
                                                    driver_description,        \
                                                .version = driver_version,     \
                                                .license = driver_license,     \
                                                .init = init_function,         \
                                                .exit = exit_function,         \
                                                .init_order = driver_order,    \
                                                .initialized = false}

extern struct driver __drivers_start[];
extern struct driver __drivers_end[];

#define DRV_FOREACH(name)                                                      \
  for (struct driver * (name) = __drivers_start; (name) < __drivers_end;       \
       ++(name))

#endif
