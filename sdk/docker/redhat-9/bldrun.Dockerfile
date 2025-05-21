FROM redhat/ubi9:9.5-1741850090

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

#hadolint ignore=DL3008
RUN dnf update -y && \
    dnf install -y --setopt=tsflags=nodocs \
    gcc-c++ procps-ng cmake pkgconfig wget ninja-build python3.12 git vim && \
    dnf clean all

#
# Install the essential packages from oneAPI to build pti
# instead of intel-oneapi-base-toolkit-2025.1.0
#
RUN echo '[oneAPI]' > /etc/yum.repos.d/oneAPI.repo; \
    echo 'name=Intel® oneAPI repository' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'baseurl=https://yum.repos.intel.com/oneapi' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'enabled=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'gpgcheck=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'repo_gpgcheck=1' >> /etc/yum.repos.d/oneAPI.repo; \
    echo 'gpgkey=https://yum.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB' >> /etc/yum.repos.d/oneAPI.repo && \
    dnf install -y intel-dpcpp-cpp-compiler-2025.1 \
                    intel-oneapi-mkl-devel-2025.1 \
                    intel-oneapi-dnnl-devel-2025.1 \
                    intel-oneapi-ccl-devel-2021.15 && \
    dnf clean all

#
# Setup the appropriate repos for GPU
#
RUN dnf install -y 'dnf-command(config-manager)' && \
    dnf config-manager --add-repo https://repositories.intel.com/gpu/rhel/9.5/unified/intel-gpu-9.5.repo && \
    dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm && \
    dnf config-manager --disable epel && \
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

RUN update-alternatives --install /usr/local/bin/python python /usr/bin/python3.12 10
