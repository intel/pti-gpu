# syntax=docker/dockerfile:1.3

FROM rockylinux:8.9

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /tmp

USER root

RUN dnf -y --setopt=tsflags=nodocs --nogpgcheck update && \
    dnf install -y --setopt=tsflags=nodocs --nogpgcheck 'dnf-command(config-manager)' && \
    dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm && \
    dnf install -y --setopt=tsflags=nodocs --nogpgcheck dnf-plugins-core && \
    dnf config-manager --set-enabled powertools && \
    dnf -y --setopt=tsflags=nodocs --nogpgcheck update && \
    dnf clean all

#hadolint ignore=DL3008
RUN dnf install -y --setopt=tsflags=nodocs  \
    gcc-c++ \
    procps-ng \
    cmake \
    wget \
    ninja-build \
    which \
    git \
    vim \
    findutils \
    sudo \
    python3.12 && \
    dnf clean all

RUN ln -s /usr/bin/python3.12 /usr/bin/python
