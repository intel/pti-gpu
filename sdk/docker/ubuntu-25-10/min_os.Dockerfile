# syntax=docker/dockerfile:1.4

# hadolint ignore=DL3007

# This is ubuntu:25.10
FROM ubuntu:questing

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
    python3.13 \
    python3-pip \
    python3.13-venv \
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
RUN apt-get update && apt-get install -y software-properties-common && \
    add-apt-repository -y ppa:kobuk-team/intel-graphics

RUN update-alternatives --install /usr/local/bin/python python /usr/bin/python3 10

