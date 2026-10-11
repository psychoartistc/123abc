#include <utils/branching.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include <utils/rdfile.h>

void pr_file(const char *path) {
  struct limine_module_response *resp = module_request.response;
  if (unlikely(!resp))
    return;

  for (size_t i = 0; i < resp->module_count; i++) {
    struct limine_file *file = resp->modules[i];
    if (unlikely(!file))
      continue;

    if (strstr(file->path, path) == NULL)
      continue;

    printk("%s", (const char *)file->address);
    break;
  }
}
