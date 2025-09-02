# syntax=docker/dockerfile:1.3

# hadolint ignore=DL3007

ARG MIN_OS_CONTAINER
FROM ${MIN_OS_CONTAINER}

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

USER root

#
# Install the essential packages from oneAPI to build pti
# instead of intel-oneapi-base-toolkit
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
    intel-dpcpp-cpp-compiler-2025.2 \
    intel-oneapi-mkl-devel-2025.2 \
    intel-oneapi-dnnl-devel-2025.2 \
    intel-oneapi-ccl-devel-2021.16 && \
    apt-get clean -y
