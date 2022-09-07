/**
 * Copyright (C) Mellanox Technologies Ltd. 2022.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "config.h"
#include "../tl_shm.h"
#include "allreduce.h"

enum
{
    ALLREDUCE_STAGE_START,
    ALLREDUCE_STAGE_BASE_TREE_REDUCE,
    ALLREDUCE_STAGE_TOP_TREE_REDUCE,
    ALLREDUCE_STAGE_BASE_TREE_BCAST,
    ALLREDUCE_STAGE_TOP_TREE_BCAST,
    ALLREDUCE_STAGE_BCAST_COPY_OUT,
    ALLREDUCE_STAGE_BCAST_READ_CHECK,
};

static inline void
ucc_tl_shm_allreduce_bcast_prep(ucc_coll_args_t   *args,
                                ucc_tl_shm_task_t *task,
                                ucc_tl_shm_tree_t *bcast_tree) {
    task->src_buf = args->src.info.buffer;
    args->src.info.buffer = args->dst.info.buffer; /* needed to fit bcast api */
    task->stage = bcast_tree->top_tree ? ALLREDUCE_STAGE_TOP_TREE_BCAST :
                                         ALLREDUCE_STAGE_BASE_TREE_BCAST;
    task->tree = bcast_tree;
    task->seq_num++; /* finished reduce, need seq_num to be updated for bcast */
}

static void ucc_tl_shm_allreduce_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_shm_task_t *task        = ucc_derived_of(coll_task,
                                                    ucc_tl_shm_task_t);
    ucc_tl_shm_team_t *team        = TASK_TEAM(task);
    ucc_coll_args_t   *args        = &TASK_ARGS(task);
    ucc_rank_t         rank        = UCC_TL_TEAM_RANK(team);
    ucc_rank_t         root        = (ucc_rank_t)args->root;
    ucc_tl_shm_seg_t  *seg         = task->seg;
    ucc_tl_shm_tree_t *bcast_tree  = task->allreduce.bcast_tree;
    ucc_tl_shm_tree_t *reduce_tree = task->allreduce.reduce_tree;
    ucc_memory_type_t  mtype       = args->dst.info.mem_type;
    ucc_datatype_t     dt          = args->dst.info.datatype;
    size_t             count       = args->dst.info.count;
    size_t             data_size   = count * ucc_dt_size(dt);
    int                is_inline   = data_size <= team->max_inline;
    int                is_op_root  = rank == root;
    ucc_tl_shm_ctrl_t *my_ctrl;

next_stage:
    switch (task->stage) {
    case ALLREDUCE_STAGE_START:
        /* checks if previous collective has completed on the seg
           TODO: can be optimized if we detect bcast->reduce pattern.*/
        SHMCHECK_GOTO(ucc_tl_shm_check_seg_ready(task, reduce_tree, 1), task,
                      out);
        if (reduce_tree->base_tree) {
            task->stage = ALLREDUCE_STAGE_BASE_TREE_REDUCE;
        } else {
            task->stage = ALLREDUCE_STAGE_TOP_TREE_REDUCE;
        }
        if (UCC_IS_INPLACE(*args) && !is_op_root) {
            args->src.info.buffer = args->dst.info.buffer;
        }
        goto next_stage;
    case ALLREDUCE_STAGE_BASE_TREE_REDUCE:
        // coverity[var_deref_model]
        SHMCHECK_GOTO(ucc_tl_shm_reduce_read(team, seg, task, reduce_tree->base_tree,
                      is_inline, count, dt, mtype, args), task, out);
        task->cur_child = 0;
        if (reduce_tree->top_tree) {
            task->stage = ALLREDUCE_STAGE_TOP_TREE_REDUCE;
        } else {
            ucc_tl_shm_allreduce_bcast_prep(args, task, bcast_tree);
        }
        goto next_stage;
    case ALLREDUCE_STAGE_TOP_TREE_REDUCE:
        // coverity[var_deref_model]
        SHMCHECK_GOTO(ucc_tl_shm_reduce_read(team, seg, task, reduce_tree->top_tree,
                      is_inline, count, dt, mtype, args), task, out);
        ucc_tl_shm_allreduce_bcast_prep(args, task, bcast_tree);
        goto next_stage;
    case ALLREDUCE_STAGE_TOP_TREE_BCAST:
        if (task->progress_alg == BCAST_WW || task->progress_alg == BCAST_WR) {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_write(team, seg, task, bcast_tree->top_tree,
                          is_inline, data_size), task, out);
        } else {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_read(team, seg, task, bcast_tree->top_tree,
                          is_inline, &is_op_root, data_size), task, out);
        }
        if (bcast_tree->base_tree) {
            task->stage = ALLREDUCE_STAGE_BASE_TREE_BCAST;
        } else {
            task->stage = ALLREDUCE_STAGE_BCAST_COPY_OUT;
        }
        goto next_stage;
    case ALLREDUCE_STAGE_BASE_TREE_BCAST:
        if (task->progress_alg == BCAST_WW || task->progress_alg == BCAST_RW) {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_write(team, seg, task, bcast_tree->base_tree,
                          is_inline, data_size), task, out);
        } else {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_read(team, seg, task, bcast_tree->base_tree,
                          is_inline, &is_op_root, data_size), task, out);
        }
        /* fall through */
    case ALLREDUCE_STAGE_BCAST_COPY_OUT:
        ucc_tl_shm_bcast_copy_out(task, data_size);
        task->cur_child = 0;
        task->stage = ALLREDUCE_STAGE_BCAST_READ_CHECK;
        /* fall through */
    case ALLREDUCE_STAGE_BCAST_READ_CHECK:
        SHMCHECK_GOTO(ucc_tl_shm_bcast_check_read_ready(task), task, out);
        break;
    }

    /* task->seq_num was updated between reduce and bcast, needs to be reset
       to fit general collectives order, as allreduce is a single collective */
    task->seq_num--;
    my_ctrl               = ucc_tl_shm_get_ctrl(seg, team, rank);
    my_ctrl->ci           = task->seq_num;
    my_ctrl->reserved     = 0; //for n_concurrent=1 in reduce, so parent will wait for children
    args->src.info.buffer = task->src_buf;
    /* allreduce done */
    task->super.status = UCC_OK;
    UCC_TL_SHM_PROFILE_REQUEST_EVENT(coll_task,
                                     "shm_allreduce_progress_done", 0);
out:
    return;
}

static ucc_status_t ucc_tl_shm_allreduce_start(ucc_coll_task_t *coll_task)
{
    ucc_tl_shm_task_t *task = ucc_derived_of(coll_task, ucc_tl_shm_task_t);
    ucc_tl_shm_team_t *team = TASK_TEAM(task);
    ucc_status_t       status;

    ucc_tl_shm_task_reset(task, team, UCC_RANK_INVALID);
    task->stage = ALLREDUCE_STAGE_START;
    task->tree  = task->allreduce.reduce_tree;
    UCC_TL_SHM_PROFILE_REQUEST_EVENT(coll_task, "shm_allreduce_start", 0);

    status = ucc_coll_task_get_executor(coll_task, &task->executor);
    if (ucc_unlikely(status != UCC_OK)) {
        return status;
    }
    task->super.status = UCC_INPROGRESS;
    return ucc_progress_queue_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
}

ucc_status_t ucc_tl_shm_allreduce_init(ucc_base_coll_args_t *coll_args,
                                       ucc_base_team_t *     tl_team,
                                       ucc_coll_task_t **    task_h)
{
    ucc_tl_shm_team_t     *team = ucc_derived_of(tl_team, ucc_tl_shm_team_t);
    ucc_tl_shm_task_t     *task;
    ucc_tl_shm_pp_bcast_t  params_bcast;
    ucc_tl_shm_pp_reduce_t params_reduce;
    ucc_status_t           status;

    if (coll_args->args.op == UCC_OP_AVG) {
        return UCC_ERR_NOT_SUPPORTED;
    }

    task = ucc_tl_shm_get_task(coll_args, team);

    if (ucc_unlikely(!task)) {
        return UCC_ERR_NO_MEMORY;
    }

    TASK_ARGS(task).root = 0;
    team->perf_params_bcast(&params_bcast.super, task);
    task->progress_alg = params_bcast.progress_alg;

    status = ucc_tl_shm_tree_init(
        team, TASK_ARGS(task).root, params_bcast.super.base_radix,
        params_bcast.super.top_radix,
        UCC_COLL_TYPE_BCAST, params_bcast.super.base_tree_only,
        &task->allreduce.bcast_tree);

    if (ucc_unlikely(UCC_OK != status)) {
        tl_error(UCC_TL_TEAM_LIB(team), "failed to init shm bcast tree");
        return status;
    }

    team->perf_params_reduce(&params_reduce.super, task);

    status = ucc_tl_shm_tree_init(
        team, TASK_ARGS(task).root, params_reduce.super.base_radix,
        params_reduce.super.top_radix,
        UCC_COLL_TYPE_REDUCE, params_reduce.super.base_tree_only,
        &task->allreduce.reduce_tree);

    if (ucc_unlikely(UCC_OK != status)) {
        tl_error(UCC_TL_TEAM_LIB(team), "failed to init shm reduce tree");
        return status;
    }

    task->super.flags   |= UCC_COLL_TASK_FLAG_EXECUTOR;

    // coverity[uninit_use:FALSE]
    task->super.post     = ucc_tl_shm_allreduce_start;
    task->super.progress = ucc_tl_shm_allreduce_progress;
    *task_h              = &task->super;
    return UCC_OK;
}
