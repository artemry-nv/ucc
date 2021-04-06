/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef UCC_TL_UCP_H_
#define UCC_TL_UCP_H_
#include "components/tl/ucc_tl.h"
#include "components/tl/ucc_tl_log.h"
#include "core/ucc_ee.h"
#include "utils/ucc_mpool.h"
#include "tl_ucp_ep_hash.h"
#include <ucp/api/ucp.h>
#include <ucs/memory/memory_type.h>

#ifndef UCC_TL_UCP_DEFAULT_SCORE
#define UCC_TL_UCP_DEFAULT_SCORE 10
#endif

typedef struct ucc_tl_ucp_iface {
    ucc_tl_iface_t super;
} ucc_tl_ucp_iface_t;
/* Extern iface should follow the pattern: ucc_tl_<tl_name> */
extern ucc_tl_ucp_iface_t ucc_tl_ucp;

typedef struct ucc_tl_ucp_lib_config {
    ucc_tl_lib_config_t super;
    uint32_t            barrier_kn_radix;
    uint32_t            allreduce_kn_radix;
    uint32_t            bcast_kn_radix;
    uint32_t            alltoall_pairwise_num_posts;
    uint32_t            alltoallv_pairwise_num_posts;
} ucc_tl_ucp_lib_config_t;

typedef struct ucc_tl_ucp_context_config {
    ucc_tl_context_config_t super;
    uint32_t                preconnect;
    uint32_t                n_polls;
    uint32_t                oob_npolls;
    uint32_t                pre_reg_mem;
} ucc_tl_ucp_context_config_t;

typedef struct ucc_tl_ucp_lib {
    ucc_tl_lib_t            super;
    ucc_tl_ucp_lib_config_t cfg;
} ucc_tl_ucp_lib_t;
UCC_CLASS_DECLARE(ucc_tl_ucp_lib_t, const ucc_base_lib_params_t *,
                  const ucc_base_config_t *);

typedef struct ucc_tl_ucp_addr_storage ucc_tl_ucp_addr_storage_t;

typedef struct ucc_tl_ucp_ep_close_state {
    void      *close_req;
} ucc_tl_ucp_ep_close_state_t;

typedef struct ucc_tl_ucp_context {
    ucc_tl_context_t            super;
    ucc_tl_ucp_context_config_t cfg;
    ucp_context_h               ucp_context;
    ucp_worker_h                ucp_worker;
    size_t                      ucp_addrlen;
    ucp_address_t              *worker_address;
    ucc_tl_ucp_ep_close_state_t ep_close_state;
    ucc_mpool_t                 req_mp;
    tl_ucp_ep_hash_t           *ep_hash;
} ucc_tl_ucp_context_t;
UCC_CLASS_DECLARE(ucc_tl_ucp_context_t, const ucc_base_context_params_t *,
                  const ucc_base_config_t *);

typedef struct ucc_tl_ucp_task ucc_tl_ucp_task_t;
typedef struct ucc_tl_ucp_team {
    ucc_tl_team_t              super;
    ucc_status_t               status;
    ucc_rank_t                 size;
    ucc_rank_t                 rank;
    ucc_tl_ucp_addr_storage_t *addr_storage;
    uint32_t                   id;
    uint32_t                   scope;
    uint32_t                   scope_id;
    uint32_t                   seq_num;
    ucc_tl_ucp_task_t         *preconnect_task;
} ucc_tl_ucp_team_t;
UCC_CLASS_DECLARE(ucc_tl_ucp_team_t, ucc_base_context_t *,
                  const ucc_base_team_params_t *);

#define UCC_TL_UCP_SUPPORTED_COLLS                         \
    (UCC_COLL_TYPE_ALLTOALL  | UCC_COLL_TYPE_ALLTOALLV  |  \
     UCC_COLL_TYPE_ALLGATHER | UCC_COLL_TYPE_ALLGATHERV |  \
     UCC_COLL_TYPE_ALLREDUCE | UCC_COLL_TYPE_BCAST      |  \
     UCC_COLL_TYPE_BARRIER)

#define UCC_TL_UCP_TEAM_LIB(_team)                                             \
    (ucc_derived_of((_team)->super.super.context->lib, ucc_tl_ucp_lib_t))

#define UCC_TL_UCP_TEAM_CTX(_team)                                             \
    (ucc_derived_of((_team)->super.super.context, ucc_tl_ucp_context_t))

#define UCC_TL_UCP_TEAM_CORE_CTX(_team)                                        \
    ((_team)->super.super.context->ucc_context)

#define UCC_TL_UCP_WORKER(_team) UCC_TL_UCP_TEAM_CTX(_team)->ucp_worker

#define UCC_TL_CTX_HAS_OOB(_ctx) ((_ctx)->super.super.ucc_context->params.mask & \
                                  UCC_CONTEXT_PARAM_FIELD_OOB)

#define UCC_TL_CTX_OOB(_ctx) ((_ctx)->super.super.ucc_context->params.oob)

void ucc_tl_ucp_pre_register_mem(ucc_tl_ucp_team_t *team, void *addr,
                                 size_t length, ucc_memory_type_t mem_type);
#endif
