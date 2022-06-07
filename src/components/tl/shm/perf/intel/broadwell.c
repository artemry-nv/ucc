/**
 * Copyright (C) Mellanox Technologies Ltd. 2022.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "../tl_shm_coll_perf_params.h"

static void
ucc_tl_shm_pp_intel_broadwell_2_14_bcast(ucc_tl_shm_perf_params_t *params,
                                         ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_bcast_t *p = ucc_derived_of(params, ucc_tl_shm_pp_bcast_t);

    if (data_size <= team->max_inline) {
        p->progress_alg         = BCAST_WR;
        p->super.base_tree_only = 0;
        p->super.base_radix     = 4;
        p->super.top_radix      = TASK_LIB(task)->cfg.bcast_top_radix;
    } else {
        p->progress_alg         = BCAST_WR;
        p->super.base_tree_only = 0;
        p->super.base_radix     = 14;
        p->super.top_radix      = TASK_LIB(task)->cfg.bcast_top_radix;
    }
}

static void
ucc_tl_shm_pp_intel_broadwell_1_14_bcast(ucc_tl_shm_perf_params_t *params,
                                         ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_bcast_t *p = ucc_derived_of(params, ucc_tl_shm_pp_bcast_t);

    if (data_size <= team->max_inline) {
        p->progress_alg         = BCAST_WR;
        p->super.base_tree_only = 1;
        p->super.base_radix     = 7;
        p->super.top_radix      = 0;
    } else {
        p->progress_alg         = BCAST_WR;
        p->super.base_tree_only = 1;
        p->super.base_radix     = 8;
        p->super.top_radix      = 0;
    }
}

static void
ucc_tl_shm_pp_intel_broadwell_1_8_bcast(ucc_tl_shm_perf_params_t *params,
                                        ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_bcast_t *p = ucc_derived_of(params, ucc_tl_shm_pp_bcast_t);

    if (data_size <= team->max_inline) {
        p->progress_alg         = BCAST_WW;
        p->super.base_tree_only = 1;
        p->super.base_radix     = 7;
        p->super.top_radix      = 0;
    } else {
        p->progress_alg         = BCAST_WR;
        p->super.base_tree_only = 1;
        p->super.base_radix     = 7;
        p->super.top_radix      = 0;
    }
}

static void
ucc_tl_shm_pp_intel_broadwell_2_14_reduce(ucc_tl_shm_perf_params_t *params,
                                          ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_reduce_t *p = ucc_derived_of(params, ucc_tl_shm_pp_reduce_t);

    if (data_size <= team->max_inline) {
        p->super.base_tree_only = 0;
        p->super.base_radix     = 8;
        p->super.top_radix      = TASK_LIB(task)->cfg.reduce_top_radix;
    } else {
        p->super.base_tree_only = 0;
        p->super.base_radix     = 2;
        p->super.top_radix      = TASK_LIB(task)->cfg.reduce_top_radix;
    }
}

static void
ucc_tl_shm_pp_intel_broadwell_1_14_reduce(ucc_tl_shm_perf_params_t *params,
                                          ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_reduce_t *p = ucc_derived_of(params, ucc_tl_shm_pp_reduce_t);

    if (data_size <= team->max_inline) {
        p->super.base_tree_only = 1;
        p->super.base_radix     = 4;
        p->super.top_radix      = 0;
    } else {
        p->super.base_tree_only = 1;
        p->super.base_radix     = 2;
        p->super.top_radix      = 0;
    }
}

static void
ucc_tl_shm_pp_intel_broadwell_1_8_reduce(ucc_tl_shm_perf_params_t *params,
                                         ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_reduce_t *p = ucc_derived_of(params, ucc_tl_shm_pp_reduce_t);

    if (data_size <= team->max_inline) {
        p->super.base_tree_only = 1;
        p->super.base_radix     = 7;
        p->super.top_radix      = 0;
    } else {
        p->super.base_tree_only = 1;
        p->super.base_radix     = 2;
        p->super.top_radix      = 0;
    }
}

ucc_tl_shm_perf_key_t intel_broadwell_2_14 =
    TL_SHM_PERF_KEY_DECLARE(INTEL, BROADWELL, "intel_broadwell_2_14",
                            ucc_tl_shm_pp_intel_broadwell_2_14_bcast,
                            ucc_tl_shm_pp_intel_broadwell_2_14_reduce,
                            SEG_LAYOUT_CONTIG, 2, 14, 14);

ucc_tl_shm_perf_key_t intel_broadwell_1_14 =
    TL_SHM_PERF_KEY_DECLARE(INTEL, BROADWELL, "intel_broadwell_1_14",
                            ucc_tl_shm_pp_intel_broadwell_1_14_bcast,
                            ucc_tl_shm_pp_intel_broadwell_1_14_reduce,
                            SEG_LAYOUT_LAST, 1, 14);

ucc_tl_shm_perf_key_t intel_broadwell_1_8 =
    TL_SHM_PERF_KEY_DECLARE(INTEL, BROADWELL, "intel_broadwell_1_8",
                            ucc_tl_shm_pp_intel_broadwell_1_8_bcast,
                            ucc_tl_shm_pp_intel_broadwell_1_8_reduce,
                            SEG_LAYOUT_LAST, 1, 8);
