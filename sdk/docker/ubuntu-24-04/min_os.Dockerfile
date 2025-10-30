# syntax=docker/dockerfile:1.4

# hadolint ignore=DL3007

# This is ubuntu:24.04
FROM ubuntu:noble

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

USER root

#hadolint ignore=DL3008
RUN apt-get update -y && \
    apt-get upgrade --no-install-recommends  -y && \
    apt-get install --no-install-recommends -y \
    cmake \
    ninja-build \
    ncurses* \
    wget \
    gpg \
    git \
    gpg-agent \
    vim \
    make \
    sudo \
    g++ \
    python3 \
    python3-pip \
    ca-certificates && \
    apt-get clean -y


# Setup the appropriate repos for oneAPI
#
RUN wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | \
    gpg --dearmor | \
    tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null && \
    echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | \
     tee /etc/apt/sources.list.d/oneAPI.list

#
# Setup the appropriate repos for GPU
#
RUN wget -qO - https://repositories.intel.com/gpu/intel-graphics.key | \
    gpg --yes --dearmor --output /usr/share/keyrings/intel-graphics.gpg && \
    echo "deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/gpu/ubuntu noble unified" | \
    tee /etc/apt/sources.list.d/intel-gpu-noble.list

RUN update-alternatives --install /usr/local/bin/python python /usr/bin/python3 10

