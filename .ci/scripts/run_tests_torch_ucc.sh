#!/bin/bash -eEx
set -o pipefail

command -v mpirun
export UCX_WARN_UNUSED_ENV_VARS=n
ucx_info -e -u t

echo "UCC barrier"
/bin/bash ${TORCH_UCC_SRC_DIR}/test/start_test.sh ${TORCH_UCC_SRC_DIR}/test/torch_barrier_test.py --backend=gloo

echo "UCC alltoall"
/bin/bash ${TORCH_UCC_SRC_DIR}/test/start_test.sh ${TORCH_UCC_SRC_DIR}/test/torch_alltoall_test.py --backend=gloo

echo "UCC alltoallv"
/bin/bash ${TORCH_UCC_SRC_DIR}/test/start_test.sh ${TORCH_UCC_SRC_DIR}/test/torch_alltoallv_test.py --backend=gloo

echo "UCC allgather"
/bin/bash ${TORCH_UCC_SRC_DIR}/test/start_test.sh ${TORCH_UCC_SRC_DIR}/test/torch_allgather_test.py --backend=gloo

echo "UCC allreduce"
/bin/bash ${TORCH_UCC_SRC_DIR}/test/start_test.sh ${TORCH_UCC_SRC_DIR}/test/torch_allreduce_test.py --backend=gloo

echo "UCC broadcast"
/bin/bash ${TORCH_UCC_SRC_DIR}/test/start_test.sh ${TORCH_UCC_SRC_DIR}/test/torch_bcast_test.py --backend=gloo
