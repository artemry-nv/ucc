#!/bin/bash -eEx
set -o pipefail

SCRIPT_DIR="$(
    cd "$(dirname "$0")"
    pwd -P
)"
cd "${SCRIPT_DIR}"
. "${SCRIPT_DIR}/env.sh"

HOSTFILE="$1"

if [ -z "$HOSTFILE" ]; then
    echo "ERROR: HOSTFILE is not specified"
    exit 1
fi

export PATH="/usr/lib64/openmpi/bin:$PATH"
export LD_LIBRARY_PATH="/usr/lib64/openmpi/lib:${LD_LIBRARY_PATH}"

HEAD_NODE=$(head -1 "$HOSTFILE")
export HEAD_NODE
export MASTER_ADDR=${HEAD_NODE}

NP=$(wc --lines "$HOSTFILE" | awk '{print $1}')

# shellcheck disable=SC2086
mpirun \
    -np $NP \
    --hostfile ${HOSTFILE} \
    --map-by node \
    --allow-run-as-root \
    --mca plm_rsh_args '-p 12345' \
    -x PATH \
    -x LD_LIBRARY_PATH \
    hostname

# shellcheck disable=SC2086
mpirun \
    -np $NP \
    --hostfile ${HOSTFILE} \
    --map-by node \
    --allow-run-as-root \
    --mca plm_rsh_args '-p 12345' \
    -x PATH \
    -x LD_LIBRARY_PATH \
    cat /proc/1/cgroup

# shellcheck disable=SC2086
mpirun \
    -np $NP \
    --hostfile ${HOSTFILE} \
    --map-by node \
    --allow-run-as-root \
    --mca plm_rsh_args '-p 12345' \
    -x PATH \
    -x LD_LIBRARY_PATH \
    -x MASTER_ADDR \
    /opt/nvidia/torch-ucc/src/ucc/build/test/mpi/ucc_test_mpi
