FROM redhat/ubi10

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

#hadolint ignore=DL3008

#
# EXCLUDE: dnf update -y
# Do not dnf update on redhat because it will upgrade to ubi9:9.6
# and our Data Center GPU / PVC will have library dependencies problems
# with libLLVM.so.18.1
#
RUN dnf update -y && \
    dnf install -y --setopt=tsflags=nodocs \
      gcc-c++ \
      procps-ng \
      cmake \
      pkgconfig \
      wget \
      ninja-build \
      python3 \
      git \
      vim \
      sudo && \
    dnf clean all

#
# Install the essential packages from oneAPI to build pti
# instead of intel-oneapi-base-toolkit
#
RUN echo '[oneAPI]' > /etc/yum.repos.d/oneAPI.repo; \
    echo 'name=Intel® oneAPI repository' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'baseurl=https://yum.repos.intel.com/oneapi' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'enabled=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'gpgcheck=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'repo_gpgcheck=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'gpgkey=https://yum.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB' >> /etc/yum.repos.d/oneAPI.repo 

#
# Setup the appropriate repos for GPU
#
RUN rpm --import https://repositories.intel.com/gpu/intel-graphics.key && \
    echo "priority=98" >> /etc/yum.repos.d/intel-gpu-10.0.repo && \
    dnf install -y 'dnf-command(config-manager)' && \
    dnf config-manager --add-repo https://repositories.intel.com/gpu/rhel/10.0/lts/2523/unified/intel-gpu-10.0.repo && \
    dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-10.noarch.rpm && \
    dnf config-manager --disable epel && \
    dnf install -y \
        intel-opencl intel-media libmfxgen1 libvpl2 \
        level-zero intel-level-zero-gpu mesa-dri-drivers mesa-vulkan-drivers \
        mesa-vdpau-drivers libdrm mesa-libEGL mesa-libgbm mesa-libGL \
        mesa-libxatracker libvpl-tools intel-metrics-discovery \
        intel-metrics-library intel-igc-core intel-igc-cm \
        libva libva-utils intel-gmmlib libmetee intel-gsc intel-ocloc \
        intel-metrics-discovery intel-metrics-discovery-devel \
        intel-metrics-library intel-metrics-library-devel && \
    dnf clean all

RUN update-alternatives --install /usr/local/bin/python python /usr/bin/python3 10
