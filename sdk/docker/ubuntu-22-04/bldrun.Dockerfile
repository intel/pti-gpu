# hadolint ignore=DL3007

# This is ubuntu:22.4
FROM ubuntu:jammy-20250126

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
    git \
    gpg-agent \
    vim \
    make \
    ca-certificates && \
    apt-get clean -y

#
# Setup the appropriate repos for oneAPI
#
RUN wget -O- --progress=dot:giga  https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | \
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

#
# Install the essential packages from oneAPI to build pti
# instead of intel-oneapi-base-toolkit-2025.1.0
#
RUN apt update -y && \
    apt install -y \
    intel-opencl-icd libze-intel-gpu1 libze1 \
    intel-media-va-driver-non-free libmfx-gen1 libvpl2 \
    libegl-mesa0 libegl1-mesa libegl1-mesa-dev libgbm1 libgl1-mesa-dev libgl1-mesa-dri \
    libglapi-mesa libglx-mesa0 libigdgmm12 libxatracker2 mesa-va-drivers \
    mesa-vdpau-drivers mesa-vulkan-drivers va-driver-all vainfo hwinfo clinfo intel-ocloc \
    libigc-dev intel-igc-cm libigdfcl-dev libigfxcmrt-dev libze-dev \
    intel-metrics-discovery intel-metrics-discovery-dev \
    intel-metrics-library intel-metrics-library-dev \
    intel-dpcpp-cpp-compiler-2025.1 \
    intel-oneapi-mkl-devel-2025.1 \
    intel-oneapi-dnnl-devel-2025.1 \
    intel-oneapi-ccl-devel-2021.15 && \
    apt-get clean -y

RUN update-alternatives --install /usr/local/bin/python python /usr/bin/python3.10 10

#
