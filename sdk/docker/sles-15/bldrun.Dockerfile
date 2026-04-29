# syntax=docker/dockerfile:1.3

# hadolint ignore=DL3007

ARG MIN_OS_CONTAINER
FROM ${MIN_OS_CONTAINER}


SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

#
# Install the essential packages from oneAPI to build pti
# instead of intel-oneapi-base-toolkit
#
RUN zypper refresh && \
  zypper up -y && \
  zypper --non-interactive install -y \
    intel-level-zero-gpu level-zero intel-gsc intel-opencl intel-ocloc \
    intel-media-driver libigfxcmrt7 libvpl2 libvpl-tools libmfxgen1 \
    libigdfcl-devel intel-igc-cm libigfxcmrt-devel level-zero-devel \
    libOpenCL1 libva-utils hwinfo \
    intel-metrics-discovery intel-metrics-discovery-devel \
    intel-metrics-library intel-metrics-library-devel \
    intel-dpcpp-cpp-compiler-2026.0 \
    intel-oneapi-mkl-devel-2026.0 \
    intel-oneapi-dnnl-devel-2026.0 \
    intel-oneapi-ccl-devel-2022.0 && \
  zypper clean --all
