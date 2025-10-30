# syntax=docker/dockerfile:1.3

FROM registry.suse.com/suse/sle15

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

#hadolint ignore=DL3041
RUN  zypper refresh && \
     zypper --non-interactive install -y \
      gawk \
      wget \
      cmake \
      gcc \
      gcc-c++ \
      ninja \
      sudo \
      awk \
      libprocps8 \
      libsystemd0 \
      procps \
      which \
      git \
      vim \
      python3 && \
     zypper clean --all

RUN zypper addrepo https://yum.repos.intel.com/oneapi oneAPI && \
  rpm --import https://yum.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB && \
  zypper addrepo -f -r https://repositories.intel.com/gpu/sles/15sp7/lts/2523/unified/intel-gpu-15sp7.repo && \
  rpm --import https://repositories.intel.com/gpu/intel-graphics.key

RUN update-alternatives --install /usr/local/bin/python python /usr/bin/python3 10
