FROM redhat/ubi9:9.4-1214

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

#hadolint ignore=DL3008
RUN dnf update -y && \
    dnf install -y --setopt=tsflags=nodocs \
    gcc-c++ procps-ng cmake wget ninja-build python3.12 && dnf clean all

#
# Install oneAPI
#
RUN echo '[oneAPI]' > /etc/yum.repos.d/oneAPI.repo; \
    echo 'name=Intel® oneAPI repository' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'baseurl=https://yum.repos.intel.com/oneapi' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'enabled=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'gpgcheck=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'repo_gpgcheck=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'gpgkey=https://yum.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB' >> /etc/yum.repos.d/oneAPI.repo && \
    dnf install -y intel-basekit-2025.0.0-884

#
# Setup the appropriate repos for GPU
#
RUN wget https://repositories.intel.com/gpu/rhel/9.4/intel-gpu-rhel-9.4.run && \
  chmod +x intel-gpu-rhel-9.4.run && \
  ./intel-gpu-rhel-9.4.run && \
  dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm

RUN dnf install -y --setopt=tsflags=nodocs \
  intel-opencl intel-media libmfxgen1 libvpl2 \
  level-zero intel-level-zero-gpu mesa-dri-drivers mesa-vulkan-drivers \
  mesa-vdpau-drivers libdrm mesa-libEGL mesa-libgbm mesa-libGL \
  mesa-libxatracker libvpl-tools \
  intel-igc-core intel-igc-cm \
  libva libva-utils intel-gmmlib libmetee intel-gsc intel-ocloc \
  intel-igc-opencl-devel level-zero-devel intel-gsc-devel libmetee-devel \
  level-zero-devel \
  intel-metrics-discovery \
  intel-metrics-discovery-devel \
  intel-metrics-library \
  intel-metrics-library-devel && \
  dnf clean all

