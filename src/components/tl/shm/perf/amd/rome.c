/**
 * Copyright (C) Mellanox Technologies Ltd. 2022.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "../tl_shm_coll_perf_params.h"

TL_SHM_PERF_KEY_DECLARE(amd_rome_2_64, AMD, ROME,
                        BCAST_WW, 0, 4, 2, BCAST_WR, 0, 16, 2,
                        0, 2, 2, 0, 2, 2,
                        SEG_LAYOUT_SOCKET, 2, 64, 64);

TL_SHM_PERF_KEY_DECLARE(amd_rome_8_16, AMD, ROME,
                        BCAST_WW, 0, 4, 4, BCAST_WW, 0, 2, 2,
                        0, 2, 4, 0, 2, 4,
                        SEG_LAYOUT_SOCKET, 8, 16, 16, 16, 16, 16, 16, 16, 16);
