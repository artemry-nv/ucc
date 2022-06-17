/**
 * Copyright (C) Mellanox Technologies Ltd. 2022.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "config.h"
#include "../tl_shm.h"
#include "fanout.h"

enum
{
    FANOUT_STAGE_START,
    FANOUT_STAGE_BASE_TREE,
    FANOUT_STAGE_TOP_TREE,
};

static void ucc_tl_shm_fanout_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_shm_task_t *task = ucc_derived_of(coll_task, ucc_tl_shm_task_t);
    ucc_tl_shm_team_t *team = TASK_TEAM(task);
    ucc_rank_t         rank = UCC_TL_TEAM_RANK(team);
    ucc_tl_shm_seg_t * seg  = task->seg;
    ucc_tl_shm_tree_t *tree = task->tree;
    ucc_tl_shm_ctrl_t *my_ctrl;

next_stage:
    switch (task->stage) {
    case FANOUT_STAGE_START:
        if ((tree->base_tree && tree->base_tree->n_children > 0) ||
            (tree->base_tree == NULL &&
             tree->top_tree->n_children > 0)) { //similar to bcast
            /* checks if previous collective has completed on the seg
               TODO: can be optimized if we detect bcast->reduce pattern.*/
            SHMCHECK_GOTO(ucc_tl_shm_check_seg_ready(task, tree, 0), task, out);
        }
        if (tree->top_tree) {
            task->stage = FANOUT_STAGE_TOP_TREE;
        } else {
            task->stage = FANOUT_STAGE_BASE_TREE;
        }
        goto next_stage;
    case FANOUT_STAGE_TOP_TREE:
        SHMCHECK_GOTO(ucc_tl_shm_fanout_signal(team, seg, task, tree->top_tree),
                      task, out);
        if (tree->base_tree) {
            task->stage = FANOUT_STAGE_BASE_TREE;
            goto next_stage;
        }
        break;
    case FANOUT_STAGE_BASE_TREE:
        SHMCHECK_GOTO(ucc_tl_shm_fanout_signal(team, seg, task, tree->base_tree),
                      task, out);
        break;
    }

    my_ctrl     = ucc_tl_shm_get_ctrl(seg, team, rank);
    my_ctrl->ci = task->seq_num;
    /* fanout done */
    task->super.status = UCC_OK;
    UCC_TL_SHM_PROFILE_REQUEST_EVENT(coll_task, "shm_fanout_progress_done", 0);
out:
    return;
}

static ucc_status_t ucc_tl_shm_fanout_start(ucc_coll_task_t *coll_task)
{
    ucc_tl_shm_task_t *task = ucc_derived_of(coll_task, ucc_tl_shm_task_t);
    ucc_tl_shm_team_t *team = TASK_TEAM(task);

    UCC_TL_SHM_PROFILE_REQUEST_EVENT(coll_task, "shm_fanout_start", 0);
    UCC_TL_SHM_SET_SEG_READY_SEQ_NUM(task, team, UCC_RANK_INVALID);
    task->super.status = UCC_INPROGRESS;
    return ucc_progress_queue_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
}

ucc_status_t ucc_tl_shm_fanout_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *     tl_team,
                                    ucc_coll_task_t **    task_h)
{
    ucc_tl_shm_team_t *team = ucc_derived_of(tl_team, ucc_tl_shm_team_t);
    ucc_rank_t base_radix   = UCC_TL_SHM_TEAM_LIB(team)->cfg.fanout_base_radix;
    ucc_rank_t top_radix    = UCC_TL_SHM_TEAM_LIB(team)->cfg.fanout_top_radix;
    int        bto          = UCC_TL_SHM_TEAM_LIB(team)->cfg.base_tree_only;
    ucc_tl_shm_task_t *task;
    ucc_status_t       status;

    if (UCC_IS_PERSISTENT(coll_args->args)) {
        return UCC_ERR_NOT_SUPPORTED;
    }

    task = ucc_tl_shm_get_task(coll_args, team);
    if (ucc_unlikely(!task)) {
        return UCC_ERR_NO_MEMORY;
    }

    task->super.post     = ucc_tl_shm_fanout_start;
    task->super.progress = ucc_tl_shm_fanout_progress;
    task->stage          = FANOUT_STAGE_START;

    status = ucc_tl_shm_tree_init(
        team, coll_args->args.root, base_radix, top_radix,
        UCC_COLL_TYPE_FANOUT, bto, &task->tree);

    if (ucc_unlikely(UCC_OK != status)) {
        tl_error(UCC_TL_TEAM_LIB(team), "failed to init shm tree");
        return status;
    }
    *task_h = &task->super;
    return UCC_OK;
}
