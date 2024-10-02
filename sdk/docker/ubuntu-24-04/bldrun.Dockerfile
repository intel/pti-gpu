# hadolint ignore=DL3007
FROM ubuntu:24.04

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

USER root

#hadolint ignore=DL3008
RUN apt-get update -y && \
    apt-get upgrade --no-install-recommends  -y && \
    apt-get install --no-install-recommends -y \
    cmake \
    ninja-build \
    ncurses*\
    wget \
    gpg \
    gpg-agent \
    ca-certificates

#
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
    echo "deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/gpu/ubuntu jammy unified" | \
    tee /etc/apt/sources.list.d/intel-gpu-jammy.list

RUN apt update -y && \
    apt install -y \
    intel-basekit=2024.2.1-98 \
    intel-opencl-icd intel-level-zero-gpu libze1 \
    intel-media-va-driver-non-free libmfx1 libmfxgen1 libvpl2 \
    libegl-mesa0 libegl1-mesa libegl1-mesa-dev libgbm1 libgl1-mesa-dev libgl1-mesa-dri \
    libglapi-mesa libgles2-mesa-dev libglx-mesa0 libigdgmm12 libxatracker2 mesa-va-drivers \
    mesa-vdpau-drivers mesa-vulkan-drivers va-driver-all vainfo hwinfo clinfo \
    libigc-dev intel-igc-cm libigdfcl-dev libigfxcmrt-dev libze-dev
