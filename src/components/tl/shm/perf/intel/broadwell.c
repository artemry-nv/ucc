/**
 * Copyright (C) Mellanox Technologies Ltd. 2022.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "../tl_shm_coll_perf_params.h"

TL_SHM_PERF_KEY_DECLARE(intel_broadwell_2_14, INTEL, BROADWELL,
                        BCAST_WW, 0, 2, 2, BCAST_WR, 0, 14, 2,
                        0, 8, 2, 0, 2, 2,
                        SEG_LAYOUT_SOCKET, 2, 14, 14);

TL_SHM_PERF_KEY_DECLARE(intel_broadwell_2_16, INTEL, BROADWELL,
                        BCAST_WW, 0, 2, 2, BCAST_WR, 0, 4, 2,
                        0, 8, 2, 0, 2, 2,
                        SEG_LAYOUT_SOCKET, 2, 16, 16);

TL_SHM_PERF_KEY_DECLARE(intel_broadwell_1_14, INTEL, BROADWELL,
                        BCAST_WR, 1, 7, 0, BCAST_WR, 1, 8, 0,
                        1, 4, 0, 1, 2, 0,
                        SEG_LAYOUT_SOCKET, 1, 14);

TL_SHM_PERF_KEY_DECLARE(intel_broadwell_1_8, INTEL, BROADWELL,
                        BCAST_WR, 1, 7, 0, BCAST_WR, 1, 7, 0,
                        1, 7, 0, 1, 2, 0,
                        SEG_LAYOUT_SOCKET, 1, 8);
