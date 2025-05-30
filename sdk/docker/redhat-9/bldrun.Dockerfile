# syntax=docker/dockerfile:1.3

ARG MIN_OS_CONTAINER
FROM ${MIN_OS_CONTAINER}

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

#
# Install the essential packages from oneAPI to build pti
# instead of intel-oneapi-base-toolkit-2025.1.0
#
RUN dnf install -y intel-dpcpp-cpp-compiler-2025.1 \
                    intel-oneapi-mkl-devel-2025.1 \
                    intel-oneapi-dnnl-devel-2025.1 \
                    intel-oneapi-ccl-devel-2021.15 && \
    dnf clean all

RUN dnf install -y \
    intel-opencl intel-media libmfxgen1 libvpl2 \
    level-zero intel-level-zero-gpu mesa-dri-drivers mesa-vulkan-drivers \
    mesa-vdpau-drivers libdrm mesa-libEGL mesa-libgbm mesa-libGL \
    mesa-libxatracker libvpl-tools intel-metrics-discovery \
    intel-metrics-library intel-igc-core intel-igc-cm \
    intel-metrics-library-devel intel-metrics-discovery-devel \
    libva libva-utils intel-gmmlib libmetee intel-gsc intel-ocloc && \
    dnf clean all
