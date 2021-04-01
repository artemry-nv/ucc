/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../mc_cuda.h"
#ifdef __cplusplus
}
#endif

__global__ void wait_kernel(volatile uint32_t *status) {
    ucc_status_t st;
    *status = UCC_MC_CUDA_TASK_STARTED;
    do {
        st = (ucc_status_t)*status;
    } while(st != UCC_MC_CUDA_TASK_COMPLETED);
    return;
}

#ifdef __cplusplus
extern "C" {
#endif

ucc_status_t ucc_mc_cuda_post_kernel_stream_task(uint32_t *status,
                                                 cudaStream_t stream)
{
    wait_kernel<<<1, 1, 0, stream>>>(status);
    CUDACHECK(cudaGetLastError());
    return UCC_OK;
}

#ifdef __cplusplus
}
#endif
