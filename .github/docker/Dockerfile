FROM nvidia/cuda:11.2.0-cudnn8-devel-ubuntu20.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
         build-essential \
         environment-modules \
         cmake \
         git \
         curl \
         ca-certificates \
         wget \
         lsb-core \
         pciutils \
         libcap2 \
         libtool-bin \
         iputils-ping \
         net-tools \
         ethtool \
         perl \
         lsb-release \
         iproute2 \
         pciutils \
         libnuma1 \
         libnuma-dev \
         numactl \
         software-properties-common \
         doxygen \
         doxygen-latex \
         bear && \
     rm -rf /var/lib/apt/lists/*

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key| apt-key add - && \
    apt-add-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-12 main" && \
    apt-get install -y --no-install-recommends clang-tidy-12
