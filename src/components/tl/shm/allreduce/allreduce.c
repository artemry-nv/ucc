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
    ALLREDUCE_STAGE_BCAST_BASE_CI,
    ALLREDUCE_STAGE_BCAST_TOP_CI,
};

static void ucc_tl_shm_allreduce_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_shm_task_t *task        = ucc_derived_of(coll_task,
                                                    ucc_tl_shm_task_t);
    ucc_tl_shm_team_t *team        = TASK_TEAM(task);
    ucc_coll_args_t   *args        = &TASK_ARGS(task);
    ucc_rank_t         rank        = UCC_TL_TEAM_RANK(team);
    ucc_rank_t         root        = task->root;
    ucc_tl_shm_seg_t  *seg         = task->seg;
    ucc_tl_shm_tree_t *bcast_tree  = task->allreduce.bcast_tree;
    ucc_tl_shm_tree_t *reduce_tree = task->allreduce.reduce_tree;
    ucc_memory_type_t  mtype       = args->dst.info.mem_type;
    ucc_datatype_t     dt          = args->dst.info.datatype;
    size_t             count       = args->dst.info.count;
    size_t             data_size   = count * ucc_dt_size(dt);
    int                is_inline   = data_size <= team->max_inline;
    int                is_op_root  = rank == root;
    int                i;
    ucc_rank_t         parent;
    ucc_tl_shm_ctrl_t *my_ctrl, *parent_ctrl;
    void              *src;

next_stage:
    switch (task->stage) {
    case ALLREDUCE_STAGE_START:
        /* checks if previous collective has completed on the seg
           TODO: can be optimized if we detect bcast->reduce pattern.*/
        SHMCHECK_GOTO(ucc_tl_shm_reduce_seg_ready(seg, task->seg_ready_seq_num,
                                                  team, reduce_tree), task,
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
        SHMCHECK_GOTO(ucc_tl_shm_reduce_read(team, seg, task, reduce_tree->base_tree,
                      is_inline, count, dt, mtype, args), task, out);
        task->cur_child = 0;
        if (reduce_tree->top_tree) {
            task->stage = ALLREDUCE_STAGE_TOP_TREE_REDUCE;
        } else {
            args->src.info.buffer = args->dst.info.buffer;
            task->stage = ALLREDUCE_STAGE_BASE_TREE_BCAST;
            task->tree = task->allreduce.bcast_tree;
            task->seq_num++; /* finished reduce, need seq_num to be updated for bcast */
        }
        my_ctrl = ucc_tl_shm_get_ctrl(seg, team, rank);
        goto next_stage;
    case ALLREDUCE_STAGE_TOP_TREE_REDUCE:
        SHMCHECK_GOTO(ucc_tl_shm_reduce_read(team, seg, task, reduce_tree->top_tree,
                      is_inline, count, dt, mtype, args), task, out);
        args->src.info.buffer = args->dst.info.buffer;
        task->stage = ALLREDUCE_STAGE_TOP_TREE_BCAST;
        task->tree = bcast_tree;
        task->seq_num++; /* finished reduce, need seq_num to be updated for bcast */
        goto next_stage;
    case ALLREDUCE_STAGE_TOP_TREE_BCAST:
        if (task->progress_alg == BCAST_WW || task->progress_alg == BCAST_WR) {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_write(team, seg, task, bcast_tree->top_tree,
                          is_inline, &is_op_root, data_size), task, out);
        } else {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_read(team, seg, task, bcast_tree->top_tree,
                          is_inline, &is_op_root, data_size), task, out);
        }
        if (bcast_tree->base_tree) {
            task->stage = ALLREDUCE_STAGE_BASE_TREE_BCAST;
            goto next_stage;
        }
        break;
    case ALLREDUCE_STAGE_BASE_TREE_BCAST:
        if (task->progress_alg == BCAST_WW || task->progress_alg == BCAST_RW) {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_write(team, seg, task, bcast_tree->base_tree,
                          is_inline, &is_op_root, data_size), task, out);
        } else {
            SHMCHECK_GOTO(ucc_tl_shm_bcast_read(team, seg, task, bcast_tree->base_tree,
                          is_inline, &is_op_root, data_size), task, out);
        }
        break;
    case ALLREDUCE_STAGE_BCAST_TOP_CI:
        goto ci_top;
        break;
    case ALLREDUCE_STAGE_BCAST_BASE_CI:
        goto ci_base;
        break;
    }

    /* Copy out to user dest:
       Where is data?
       If we did READ as 2nd step in bcast then the data is in the
       base_tree->parent SHM.
       If we did WRITE as 2nd step in bcast then the data is in my SHM */

    my_ctrl = ucc_tl_shm_get_ctrl(seg, team, rank);

    if (!is_op_root) {
        if (task->progress_alg == BCAST_WW || task->progress_alg == BCAST_RW) {
            src = is_inline ? my_ctrl->data
                            : ucc_tl_shm_get_data(seg, team, rank);
        } else {
            if (task->progress_alg == BCAST_RR) {
                parent = (bcast_tree->base_tree &&
                          bcast_tree->base_tree->parent != UCC_RANK_INVALID)
                             ? bcast_tree->base_tree->parent
                             : bcast_tree->top_tree->parent;
            } else {
                parent = bcast_tree->base_tree
                             ? ((bcast_tree->base_tree->parent == UCC_RANK_INVALID)
                                    ? rank
                                    : bcast_tree->base_tree->parent)
                             : rank;
            }
            parent_ctrl = ucc_tl_shm_get_ctrl(seg, team, parent);
            src         = is_inline ? parent_ctrl->data
                                    : ucc_tl_shm_get_data(seg, team, parent);
        }
        memcpy(args->dst.info.buffer, src, data_size);
        ucc_memory_cpu_store_fence();
    }

    /* Thes next conditions handle special case of potential race:
       it only can happen when algorithm is using bcast_read function,
       2 trees are used, and multiple colls are being called in a row.
       Socket leader which is not actual op root must wait for its
       children to complete reading the data from its SHM before it
       can set its own CI (signalling the seg can be re-used).

       Otherwise, parent rank of this socket leader
       (either actual root or another socket leader) may overwrite the
       SHM data in the subsequent bcast, while the data is not entirely
       copied by leafs.
    */
    if (bcast_tree->top_tree && bcast_tree->top_tree->n_children > 0 &&
        (task->progress_alg == BCAST_RW || task->progress_alg == BCAST_RR)) {
        task->stage = ALLREDUCE_STAGE_BCAST_TOP_CI;
    ci_top:
        for (i = 0; i < bcast_tree->top_tree->n_children; i++) {
            ucc_tl_shm_ctrl_t *ctrl =
                ucc_tl_shm_get_ctrl(seg, team, bcast_tree->top_tree->children[i]);
            if (ctrl->ci < task->seq_num - 1) {
                return;
            }
        }
        my_ctrl = ucc_tl_shm_get_ctrl(seg, team, rank);
    }

    if (bcast_tree->base_tree && bcast_tree->base_tree->n_children > 0 &&
        (task->progress_alg == BCAST_WR || task->progress_alg == BCAST_RR)) {
        task->stage = ALLREDUCE_STAGE_BCAST_BASE_CI;
    ci_base:
        for (i = 0; i < bcast_tree->base_tree->n_children; i++) {
            ucc_tl_shm_ctrl_t *ctrl =
                ucc_tl_shm_get_ctrl(seg, team, bcast_tree->base_tree->children[i]);
            if (ctrl->ci < task->seq_num - 1) {
                return;
            }
        }
        my_ctrl = ucc_tl_shm_get_ctrl(seg, team, rank);
    }

    /* task->seq_num was updated between reduce and bcast, needs to be reset
       to fit general collectives order, as allreduce is a single collective */
    my_ctrl->ci = task->seq_num - 1;
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

    UCC_TL_SHM_PROFILE_REQUEST_EVENT(coll_task, "shm_allreduce_start", 0);
    UCC_TL_SHM_SET_SEG_READY_SEQ_NUM(task, team);
    task->super.status = UCC_INPROGRESS;
    return ucc_progress_queue_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
}

ucc_status_t ucc_tl_shm_allreduce_init(ucc_base_coll_args_t *coll_args,
                                     ucc_base_team_t *     tl_team,
                                     ucc_coll_task_t **    task_h)
{
    ucc_tl_shm_team_t *team = ucc_derived_of(tl_team, ucc_tl_shm_team_t);
    ucc_tl_shm_task_t *task;
    ucc_status_t       status;

    if (UCC_IS_PERSISTENT(coll_args->args) ||
        coll_args->args.op == UCC_OP_AVG) {
        return UCC_ERR_NOT_SUPPORTED;
    }

    task = ucc_tl_shm_get_task(coll_args, team);
    if (ucc_unlikely(!task)) {
        return UCC_ERR_NO_MEMORY;
    }

    task->root = 0;
    team->perf_params_bcast(&task->super);
    /* values from team->perf_params_bcast(&task->super) */
    task->allreduce.bcast_base_radix      = task->base_radix;
    task->allreduce.bcast_top_radix       = task->top_radix;
    task->allreduce.bcast_base_tree_only  = task->base_tree_only;

    team->perf_params_reduce(&task->super);
    /* values from team->perf_params_reduce(&task->super) */
    task->allreduce.reduce_base_radix     = task->base_radix;
    task->allreduce.reduce_top_radix      = task->top_radix;
    task->allreduce.reduce_base_tree_only = task->base_tree_only;

    task->super.post     = ucc_tl_shm_allreduce_start;
    task->super.progress = ucc_tl_shm_allreduce_progress;
    task->stage          = ALLREDUCE_STAGE_START;

    status = ucc_tl_shm_tree_init(team, task->root,
                                  task->allreduce.bcast_base_radix,
                                  task->allreduce.bcast_top_radix,
                                  &task->tree_in_cache, UCC_COLL_TYPE_BCAST,
                                  task->allreduce.bcast_base_tree_only,
                                  &task->allreduce.bcast_tree);

    if (ucc_unlikely(UCC_OK != status)) {
        tl_error(UCC_TL_TEAM_LIB(team), "failed to init shm bcast tree");
        return status;
    }

    status = ucc_tl_shm_tree_init(team, task->root,
                                  task->allreduce.reduce_base_radix,
                                  task->allreduce.reduce_top_radix,
                                  &task->tree_in_cache, UCC_COLL_TYPE_REDUCE,
                                  task->allreduce.reduce_base_tree_only,
                                  &task->allreduce.reduce_tree);

    if (ucc_unlikely(UCC_OK != status)) {
        tl_error(UCC_TL_TEAM_LIB(team), "failed to init shm reduce tree");
        return status;
    }

    task->tree = task->allreduce.reduce_tree;
    *task_h = &task->super;
    return UCC_OK;
}
