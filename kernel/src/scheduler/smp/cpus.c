#include <arch/control.h>
#include <scheduler/smp/cpus.h>
#include <stdatomic.h>
#include <stdnoreturn.h>
#include <utils/attributes.h>
#include <utils/branching.h>
#include <utils/logging.h>

static cpus_state_t s_state = {0};

noreturn static void ap_entry1(struct limine_mp_info *info) {
  u32 idx = (u32)info->extra_argument;
  set_current_cpu(&s_state.cpus[idx]);

  atomic_fetch_add_explicit(&s_state.cpus_online, 1, memory_order_release);

  ap_entry(info);

  unreachable();
  hcf();
}

void cpus_init() {
  struct limine_mp_response *resp = mp_request.response;
  if (unlikely(!resp)) {
    // assume there is one cpu
    s_state.cpus[0].idx = 0;
    s_state.cpus[0].lapic_id = 0;

    set_current_cpu(&s_state.cpus[0]);
    atomic_fetch_add_explicit(&s_state.cpus_online, 1, memory_order_release);

    return;
  }

  u32 bspId = resp->bsp_lapic_id;

  u64 count = cpus_count();
  for (u64 i = 0; i < count; i++) {
    struct limine_mp_info *info = resp->cpus[i];

    s_state.cpus[i].idx = i;
    s_state.cpus[i].lapic_id = info->lapic_id;
    if (info->lapic_id == bspId) {
      set_current_cpu(&s_state.cpus[i]);
      atomic_fetch_add_explicit(&s_state.cpus_online, 1, memory_order_release);
      pr_info("core %zu initialized (bsp)", i);
      continue;
    }

    info->extra_argument = i;
    atomic_store_explicit(&info->goto_address, ap_entry1, memory_order_seq_cst);
  }

  while (atomic_load_explicit(&s_state.cpus_online, memory_order_acquire) <
         count)
    pause();
}
