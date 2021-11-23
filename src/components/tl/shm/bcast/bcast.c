/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "config.h"
#include "../tl_shm.h"
#include "bcast.h"

ucc_status_t ucc_tl_shm_bcast_write(ucc_tl_shm_team_t *team, ucc_tl_shm_seg_t *seg, ucc_tl_shm_task_t *task,
                                     ucc_kn_tree_t *tree, int is_inline,
                                     int is_op_root, size_t data_size)
{
	ucc_rank_t team_rank = UCC_TL_TEAM_RANK(team);
    uint32_t seq_num = task->seq_num;
    uint32_t n_polls = UCC_TL_SHM_TEAM_LIB(team)->cfg.n_polls;
    ucc_tl_shm_ctrl_t *my_ctrl; //, *ctrl;
    void *src;

    my_ctrl = ucc_tl_shm_get_ctrl(seg, team, team_rank);

    if (tree->parent == UCC_RANK_INVALID) {
        /* i am root of the tree*/
        /* If the tree root is global OP root he can copy data out from origin user src buffer.
           Otherwise, it must be base_tree in 2lvl alg, and the data of the tree root is in the
           local shm (ctrl or data) */
        src = is_op_root ? TASK_ARGS(task).src.info.buffer : (is_inline ? my_ctrl->data :
                    ucc_tl_shm_get_data(seg, team, team_rank));
        ucc_tl_shm_copy_to_children(seg, team, tree, seq_num, is_inline, src, data_size);
    } else {
        for (int i = 0; i < n_polls; i++) {
            if (my_ctrl->pi == seq_num) {
                SHMSEG_ISYNC();
                src = is_inline ? my_ctrl->data : ucc_tl_shm_get_data(seg, team, team_rank);
                ucc_tl_shm_copy_to_children(seg, team, tree, seq_num, is_inline, src, data_size); //why group_type?
                /* copy out to user dest is done in the end of base bcast alg */
                return UCC_OK;
            }
        }
        return UCC_INPROGRESS;
    }
    return UCC_OK;
}

ucc_status_t ucc_tl_shm_bcast_read(ucc_tl_shm_team_t *team, ucc_tl_shm_seg_t *seg, ucc_tl_shm_task_t *task,
                                    ucc_kn_tree_t *tree, int is_inline,
                                    int is_op_root, size_t data_size)
{
//	printf("parent = %d, n_children = %d\n",tree->parent, tree->n_children);
//	for (int i = 0; i < tree->n_children; i++) {
//		printf("child %d = %d\n", i+1, tree->children[i]);
//	}
	ucc_rank_t team_rank = UCC_TL_TEAM_RANK(team);
//	printf("tree = %p, team_rank = %d\n", tree, team_rank);
	uint32_t seq_num = task->seq_num;
    uint32_t n_polls = UCC_TL_SHM_TEAM_LIB(team)->cfg.n_polls;
    void *my_data, *src, *dst;
    ucc_tl_shm_ctrl_t *parent_ctrl, *my_ctrl;

    my_ctrl = ucc_tl_shm_get_ctrl(seg, team, team_rank);

    if (tree->parent == UCC_RANK_INVALID) {//(is_op_root) {
        /* Only global op root needs to copy the data from user src to its shm */
        my_data = ucc_tl_shm_get_data(seg, team, team_rank);
        dst = is_inline ? my_ctrl->data : my_data;
        memcpy(dst, TASK_ARGS(task).src.info.buffer, data_size);
        SHMSEG_WMB();
        ucc_tl_shm_signal_to_children(seg, team, seq_num, tree);
    } else {
        for (int i = 0; i < n_polls; i++){
            if (my_ctrl->pi == seq_num) {
                parent_ctrl = ucc_tl_shm_get_ctrl(seg, team, tree->parent);
                SHMSEG_ISYNC();
                if (tree->n_children) {
                    src = is_inline ? parent_ctrl->data : ucc_tl_shm_get_data(seg, team, tree->parent);
                    my_data = ucc_tl_shm_get_data(seg, team, team_rank);
                    dst = is_inline ? my_ctrl->data : my_data;
                    memcpy(dst, src, data_size);
                    SHMSEG_WMB();
                    ucc_tl_shm_signal_to_children(seg, team, seq_num, tree);
                }
                /* copy out to user dest is done in the end of base bcast alg */
                return UCC_OK;
            }
        }
        return UCC_INPROGRESS;
    }
    return UCC_OK;
}
ucc_status_t ucc_tl_shm_tree_init_bcast(ucc_tl_shm_team_t *team,
                                        ucc_rank_t root,
                                        ucc_rank_t base_radix,
                                        ucc_rank_t top_radix,
                                        ucc_tl_shm_tree_t **tree_p)
{
    ucc_kn_tree_t *base_tree, *top_tree;
    ucc_tl_shm_tree_t *shm_tree = (ucc_tl_shm_tree_t *) ucc_malloc(sizeof(ucc_kn_tree_t *) * 2);
    ucc_rank_t tree_root, rank;
    size_t top_tree_size, base_tree_size;
    ucc_rank_t team_rank = UCC_TL_TEAM_RANK(team);
    ucc_rank_t group_size = team->base_groups[team->my_group_id].group_size;
    ucc_rank_t leaders_size = team->leaders_group->group_size;

    if (ucc_tl_shm_cache_tree_lookup(team, base_radix, top_radix, root, UCC_COLL_TYPE_BCAST, tree_p) == 1) {
        return UCC_OK;
    }
    /* Pool is initialized using UCC_KN_TREE_SIZE macro memory estimation, using
       base_group[my_group_id]->size and max supported radix (maybe up to group size as well */
//    ucc_rank_t base_size = ucc_ep_map_eval(team->rank_group_id_map, root);
    top_tree_size = ucc_tl_shm_kn_tree_size(leaders_size, top_radix);
    base_tree_size = ucc_tl_shm_kn_tree_size(group_size, base_radix);
    base_tree = (ucc_kn_tree_t *) ucc_malloc(sizeof(ucc_rank_t) * (base_tree_size + 2));
    top_tree = (ucc_kn_tree_t *) ucc_malloc(sizeof(ucc_rank_t) * (top_tree_size + 2));
//    shm_tree = (ucc_tl_shm_tree_t *) ucc_malloc(sizeof(ucc_kn_tree_t *) *
//                                     (top_tree_size + base_tree_size));//TODO: alloc_from_shm_tree_pool() ucc_mpool_get(&ctx->req_mp);

//    if (!shm_tree) {
//        return UCC_ERR_NO_MEMORY;
//    }

    if (!base_tree || !top_tree) {
        return UCC_ERR_NO_MEMORY;
    }

    shm_tree->base_tree = NULL;
    shm_tree->top_tree = NULL; //why?

//    shm_tree->top_tree = PTR_OFFSET(shm_tree,
//                                    sizeof(ucc_kn_tree_t) * base_tree_size);

//    shm_tree->base_tree = base_tree;
//    shm_tree->top_tree = top_tree;
    printf("group_size = %d, leaders_size = %d \n", group_size, leaders_size);
    ucc_rank_t root_group = ucc_ep_map_eval(team->rank_group_id_map, root);
    ucc_sbgp_t *sbgp = &team->base_groups[team->my_group_id];
    ucc_rank_t local_rank;
    int i;
    for (i = 0; i < sbgp->group_size; i++) {
    	if (ucc_ep_map_eval(sbgp->map, i) == team_rank) {
    		local_rank = i;
    		break;
    	}
    }
    ucc_assert(i < sbgp->group_size);
    if (group_size > 1) {
//        size = team->base_groups[team->my_group_id]->group_size;
        rank = local_rank;
        if (team->my_group_id == root_group) {
            tree_root = ucc_ep_map_eval(team->group_rank_map, root);
        } else {
            tree_root = 0;
        }
        printf("before group tree_init, rank = %d, team_rank = %d\n", rank, team_rank);
        ucc_tl_shm_kn_tree_init(group_size, tree_root, rank, base_radix,
                                UCC_COLL_TYPE_BCAST, base_tree);
        shm_tree->base_tree = base_tree;
        /* Convert the tree to origin TL/TEAM ranks from the BASE_GROUP ranks*/
        ucc_tl_shm_tree_to_team_ranks(base_tree, team->base_groups[team->my_group_id].map);
    }
    if (leaders_size > 1) {
        if (team_rank == root ||
            (root_group != team->my_group_id && UCC_SBGP_ENABLED == team->leaders_group->status)) {
            /* short cut if root is part of leaders SBGP
               Loop below is up to number of sockets/numas, ie small and fast*/
            tree_root = UCC_RANK_INVALID;
            for (i = 0; i < team->leaders_group->group_size; i++) {
                if (ucc_ep_map_eval(team->leaders_group->map, i) == root) {
                    tree_root = i;
                    break;
                }
            }
//            size = team->leaders_group->group_size;
            rank = team->leaders_group->group_rank;
            if (tree_root != UCC_RANK_INVALID) {
                /* Root is part of leaders groop */
            	printf("before leaders tree_init regular, rank = %d, team_rank = %d\n", rank, team_rank);
            	ucc_tl_shm_kn_tree_init(leaders_size, tree_root, rank, top_radix,
                            UCC_COLL_TYPE_BCAST, top_tree);
                /* Convert the tree to origin TL/TEAM ranks from the BASE_GROUP ranks*/
                ucc_tl_shm_tree_to_team_ranks(top_tree, team->leaders_group->map);
            } else {
                /* Build tmp ep_map for leaders + root set
                  The Leader Rank on the same base group with actual root will be replaced in the tree
                  by the root itself to save 1 extra copy in SM */
                ucc_rank_t ranks[leaders_size]; //Can be allocated on stack
                for (i = 0; i < leaders_size; i++) {
                    ucc_rank_t leader_rank = ucc_ep_map_eval(team->leaders_group->map, i);
                    ucc_rank_t leader_group_id = ucc_ep_map_eval(team->rank_group_id_map, leader_rank);
                    if (leader_group_id == root_group) {
                        tree_root = i;
                        ucc_assert(team_rank == root);
                        ranks[i] = root;
                        rank = i;
                    } else {
                        ranks[i] = leader_rank;
                    }
                }
//                top_tree = shm_tree->top_tree;
                printf("before leaders tree_init with swap, rank = %d, team_rank = %d\n", rank, team_rank);
                ucc_tl_shm_kn_tree_init(leaders_size, tree_root, rank,
                                        UCC_COLL_TYPE_BCAST, top_radix,
                                        top_tree);
                /* Convert the tree to origin TL/TEAM ranks from the BASE_GROUP ranks*/
//                shm_tree->top_tree = top_tree;
                ucc_ep_map_t map = {
                    .type  = UCC_EP_MAP_ARRAY,
                    .array.map = ranks,
                    .array.elem_size = sizeof(ucc_rank_t)
                };
                ucc_tl_shm_tree_to_team_ranks(top_tree, map);
            }
            shm_tree->top_tree = top_tree;
        }
    }
    ucc_tl_shm_cache_tree(team, base_radix, top_radix, root, UCC_COLL_TYPE_BCAST, shm_tree);
    *tree_p = shm_tree;
    return UCC_OK;
}

ucc_status_t ucc_tl_shm_bcast_wr_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_shm_task_t *task = ucc_derived_of(coll_task, ucc_tl_shm_task_t);
    ucc_tl_shm_team_t *team = TASK_TEAM(task);
    ucc_coll_args_t    args = TASK_ARGS(task);
    size_t             data_size = args.src.info.count *
                                   ucc_dt_size(args.src.info.datatype);
    ucc_rank_t         root = (ucc_rank_t) args.root, rank = UCC_TL_TEAM_RANK(team);
    ucc_tl_shm_seg_t  *seg = task->seg;
    ucc_tl_shm_tree_t *tree = task->tree;
    int                is_inline = data_size <= team->max_inline;
    ucc_status_t       status;
    ucc_tl_shm_ctrl_t *my_ctrl, *parent_ctrl;
    void *src;

    if (rank == root) {
        /* checks if previous collective has completed on the seg
           TODO: can be optimized if we detect bcast->reduce pattern.*/
        if (UCC_OK != ucc_tl_shm_seg_ready(seg)) {
            return UCC_INPROGRESS;
        }
    }
    if (tree->top_tree) {
        status = ucc_tl_shm_bcast_write(team, seg, task, tree->top_tree, is_inline, rank == root,
                                data_size);
        if (UCC_OK != status) {
            /* in progress */
            return status;
        }
    }

    status = ucc_tl_shm_bcast_read(team, seg, task, tree->base_tree, is_inline, rank == root,
                           data_size);

    if (UCC_OK != status) {
        /* in progress */
        return status;
    }

    /* Copy out to user dest:
       Where is data?
       If we did READ as 2nd step then the data is in the base_tree->parent SHM
       If we did WRITE as 2nd step then the data is in my SHM */
    if (tree->base_tree->parent != UCC_RANK_INVALID) { //(rank != root) {
        parent_ctrl = ucc_tl_shm_get_ctrl(seg, team, tree->base_tree->parent); //base_tree? was just tree->parent before
        src = is_inline ? parent_ctrl->data : ucc_tl_shm_get_data(seg, team, tree->base_tree->parent); //base_tree? was just tree->parent before
        memcpy(args.src.info.buffer, src, data_size);
    }

    my_ctrl = ucc_tl_shm_get_ctrl(seg, team, rank);
    my_ctrl->ci = task->seq_num;
    /* bcast done */
    task->super.super.status = UCC_OK;
    return UCC_OK;
}

//ucc_status_t ucc_tl_shm_bcast_ww_progress(ucc_coll_task_t *coll_task) {
//    return UCC_OK;
//}
//
//ucc_status_t ucc_tl_shm_bcast_rr_progress(ucc_coll_task_t *coll_task) {
//    return UCC_OK;
//}
//
//ucc_status_t ucc_tl_shm_bcast_rw_progress(ucc_coll_task_t *coll_task) {
//    return UCC_OK;
//}

ucc_status_t ucc_tl_shm_bcast_start(ucc_coll_task_t *coll_task)
{
    ucc_tl_shm_task_t *task = ucc_derived_of(coll_task, ucc_tl_shm_task_t);
	ucc_tl_shm_team_t *team = TASK_TEAM(task);
//	ucc_coll_args_t    args = TASK_ARGS(task);
    ucc_status_t status;
//    size_t data_size = args.src.info.count *
//                       ucc_dt_size(args.src.info.datatype);
//    ucc_rank_t rank = UCC_TL_TEAM_RANK(team);

    task->seq_num++;
    task->super.super.status = UCC_INPROGRESS;
    status = task->super.progress(&task->super);

    if (UCC_INPROGRESS == status) {
        ucc_progress_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
        return UCC_OK;
    }
    return ucc_task_complete(coll_task);
}


ucc_status_t ucc_tl_shm_bcast_init(ucc_tl_shm_task_t *task)
{
	ucc_tl_shm_team_t *team  = TASK_TEAM(task);
	ucc_coll_args_t    args  = TASK_ARGS(task);
	ucc_rank_t         base_radix = UCC_TL_SHM_TEAM_LIB(team)->cfg.bcast_base_radix;
	ucc_rank_t         top_radix  = UCC_TL_SHM_TEAM_LIB(team)->cfg.bcast_top_radix;
	ucc_status_t       status;

    task->super.post = ucc_tl_shm_bcast_start;
    task->seq_num    = team->seq_num++;
    task->seg        = &team->segs[task->seq_num % team->n_concurrent];
    status = ucc_tl_shm_tree_init_bcast(team, args.root, base_radix,
                                        top_radix, &task->tree);
    if (ucc_unlikely(UCC_OK != status)) {
        tl_error(UCC_TL_TEAM_LIB(team), "failed to init shm tree");
    	return status;
    }
//    task->super.progress = ucc_tl_shm_bcast_progress;
    switch(TASK_LIB(task)->cfg.bcast_alg) {
//        case BCAST_WW:
//        	task->super.progress = ucc_tl_shm_bcast_ww_progress;
//            break;
        case BCAST_WR:
        	task->super.progress = ucc_tl_shm_bcast_wr_progress;
            break;
//        case BCAST_RR:
//        	task->super.progress = ucc_tl_shm_bcast_rr_progress;
//            break;
//        case BCAST_RW:
//        	task->super.progress = ucc_tl_shm_bcast_rw_progress;
//            break;
    }
    return UCC_OK;
}
