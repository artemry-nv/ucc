/**
 * Copyright (C) Mellanox Technologies Ltd. 2022.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "../tl_shm_coll_perf_params.h"

static void ucc_tl_shm_pp_amd_rome_2_64_bcast(ucc_tl_shm_perf_params_t *params,
                                              ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_bcast_t *p = ucc_derived_of(params, ucc_tl_shm_pp_bcast_t);

    if (data_size <= team->max_inline) {
        p->progress_alg         = BCAST_WW;
        p->super.base_tree_only = 0;
        p->super.base_radix     = 4;
        p->super.top_radix      = 2;
    } else {
        p->progress_alg         = BCAST_WR;
        p->super.base_tree_only = 0;
        p->super.base_radix     = 16;
        p->super.top_radix      = 2;
    }
}

static void ucc_tl_shm_pp_amd_rome_2_64_reduce(ucc_tl_shm_perf_params_t *params,
                                               ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_reduce_t *p = ucc_derived_of(params, ucc_tl_shm_pp_reduce_t);

    if (data_size <= team->max_inline) {
        p->super.base_tree_only = 0;
        p->super.base_radix     = 2;
        p->super.top_radix      = TASK_LIB(task)->cfg.reduce_top_radix;
    } else {
        p->super.base_tree_only = 0;
        p->super.base_radix     = 2;
        p->super.top_radix      = TASK_LIB(task)->cfg.reduce_top_radix;
    }
}

static void ucc_tl_shm_pp_amd_rome_4_32_bcast(ucc_tl_shm_perf_params_t *params,
                                              ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_bcast_t *p = ucc_derived_of(params, ucc_tl_shm_pp_bcast_t);

    if (data_size <= team->max_inline) {
        p->progress_alg         = BCAST_WW;
        p->super.base_tree_only = 0;
        p->super.base_radix     = 4;
        p->super.top_radix      = 4;
    } else {
        p->progress_alg         = BCAST_WW;
        p->super.base_tree_only = 0;
        p->super.base_radix     = 2;
        p->super.top_radix      = 0;
    }
}

static void ucc_tl_shm_pp_amd_rome_4_32_reduce(ucc_tl_shm_perf_params_t *params,
                                               ucc_tl_shm_task_t        *task)
{
    ucc_tl_shm_team_t *team      = TASK_TEAM(task);
    size_t             data_size = ucc_coll_args_msgsize(&task->super.bargs);
    ucc_tl_shm_pp_reduce_t *p = ucc_derived_of(params, ucc_tl_shm_pp_reduce_t);

    //TODO not tuned yet
    if (data_size <= team->max_inline) {
        p->super.base_tree_only = 0;
        p->super.base_radix     = 2;
        p->super.top_radix      = TASK_LIB(task)->cfg.reduce_top_radix;
    } else {
        p->super.base_tree_only = 0;
        p->super.base_radix     = 2;
        p->super.top_radix      = TASK_LIB(task)->cfg.reduce_top_radix;
    }
}

ucc_tl_shm_perf_key_t amd_rome_2_64 =
    TL_SHM_PERF_KEY_DECLARE(AMD, ROME, "amd_rome_2_64",
                            ucc_tl_shm_pp_amd_rome_2_64_bcast,
                            ucc_tl_shm_pp_amd_rome_2_64_reduce,
                            SEG_LAYOUT_SOCKET, 2, 64, 64);

ucc_tl_shm_perf_key_t amd_rome_4_32 =
    TL_SHM_PERF_KEY_DECLARE(AMD, ROME, "amd_rome_4_32",
                            ucc_tl_shm_pp_amd_rome_4_32_bcast,
                            ucc_tl_shm_pp_amd_rome_4_32_reduce,
                            SEG_LAYOUT_SOCKET, 4, 32, 32, 32, 32);
