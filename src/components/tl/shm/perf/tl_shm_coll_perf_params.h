/**
 * Copyright (C) Mellanox Technologies Ltd. 2022.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "../tl_shm.h"
#include "../tl_shm_coll.h"

extern ucc_tl_shm_perf_key_t intel_broadwell_2_14;
extern ucc_tl_shm_perf_key_t intel_broadwell_1_14;
extern ucc_tl_shm_perf_key_t intel_broadwell_1_8;
extern ucc_tl_shm_perf_key_t intel_skylake_2_20;
extern ucc_tl_shm_perf_key_t amd_rome_2_64;

static inline void
ucc_tl_shm_perf_params_generic_bcast(ucc_tl_shm_perf_params_t *params,
                                     ucc_tl_shm_task_t *task)
{
    ucc_tl_shm_pp_bcast_t *p = ucc_derived_of(params,
                                              ucc_tl_shm_pp_bcast_t);

    p->progress_alg         = TASK_LIB(task)->cfg.bcast_alg;
    p->super.base_tree_only = TASK_LIB(task)->cfg.base_tree_only;
    p->super.base_radix     = TASK_LIB(task)->cfg.bcast_base_radix;
    p->super.top_radix      = TASK_LIB(task)->cfg.bcast_top_radix;
}

static inline void
ucc_tl_shm_perf_params_generic_reduce(ucc_tl_shm_perf_params_t *params,
                                      ucc_tl_shm_task_t *task)
{
    ucc_tl_shm_pp_reduce_t *p = ucc_derived_of(params,
                                               ucc_tl_shm_pp_reduce_t);

    p->super.base_tree_only = TASK_LIB(task)->cfg.base_tree_only;
    p->super.base_radix     = TASK_LIB(task)->cfg.reduce_base_radix;
    p->super.top_radix      = TASK_LIB(task)->cfg.reduce_top_radix;
}

#define TL_SHM_PERF_KEY_DECLARE(_vendor, _model, _label, _bcast_fn,      \
                                _reduce_fn, _layout, _n_groups, ...)     \
    {                                                                    \
        .cpu_vendor  = UCC_CPU_VENDOR_ ## _vendor,                       \
        .cpu_model   = UCC_CPU_MODEL_ ## _vendor ## _ ## _model,         \
         .label = _label,                                                \
        .groups      = { __VA_ARGS__},                                   \
        .n_groups    = _n_groups,                                        \
        .layout      = _layout,                                          \
        .bcast_func  = _bcast_fn,                                        \
        .reduce_func = _reduce_fn}
