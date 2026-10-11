#include <drivers/api.h>
#include <utils/logging.h>

void drivers_initialize_some(int level) {
  DRV_FOREACH(drv) {
    if (drv->initialized || drv->init_order != level || !drv->init)
      continue;

    drv->init();

    drv->initialized = true;

    pr_info("\tdriver \'%s\' initialized", drv->name);
  }
}
