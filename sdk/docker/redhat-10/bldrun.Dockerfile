# syntax=docker/dockerfile:1.3

ARG MIN_OS_CONTAINER
FROM ${MIN_OS_CONTAINER}

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

#
# Install the essential packages from oneAPI to build pti
# instead of intel-oneapi-base-toolkit
#
RUN dnf install -y intel-dpcpp-cpp-compiler-2025.3 \
                    intel-oneapi-mkl-devel-2025.3 \
                    intel-oneapi-dnnl-devel-2025.3 \
                    intel-oneapi-ccl-devel-2021.17 && \
    dnf clean all

RUN dnf install  --skip-broken --nobest -y \
    intel-opencl intel-media libmfxgen1 libvpl2 \
    level-zero intel-level-zero-gpu mesa-dri-drivers mesa-vulkan-drivers \
    mesa-vdpau-drivers libdrm mesa-libEGL mesa-libgbm mesa-libGL \
    mesa-libxatracker libvpl-tools intel-metrics-discovery \
    intel-metrics-library intel-igc-core intel-igc-cm \
    intel-metrics-library-devel intel-metrics-discovery-devel \
    libva libva-utils intel-gmmlib libmetee intel-gsc intel-ocloc \
    intel-igc-opencl-devel level-zero-devel intel-gsc-devel libmetee-devel && \
    dnf install --enablerepo epel -y clinfo && \
    dnf clean all
