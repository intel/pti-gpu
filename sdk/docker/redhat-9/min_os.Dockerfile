FROM redhat/ubi9

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
      python3.12 \
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
RUN dnf install -y 'dnf-command(config-manager)' && \
    dnf config-manager --add-repo https://repositories.intel.com/gpu/rhel/9.5/unified/intel-gpu-9.5.repo && \
    dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm && \
    dnf config-manager --disable epel && \
    dnf clean all

RUN update-alternatives --install /usr/local/bin/python python /usr/bin/python3.12 10
