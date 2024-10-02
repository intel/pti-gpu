USE_BUILDKIT := 1

REL_UBUNTU:= ubuntu-24-04
REL_RHEL:= redhat-9
REL_SLES:= sles-15.3
OS_TARGETS:= ${REL_SLES} ${REL_RHEL} ${REL_UBUNTU}
.PHONY:${OS_TARGETS}

ONEAPI_VER=2024.2.1

targets: ${OS_TARGETS}
	@echo BUILT ${OS_TARGETS}

VER_UBUNTU:=01
VER_REDHAT:=01
VER_SLES:=01
define getOsVer
	$(if $(filter $1, ${REL_UBUNTU}),${VER_UBUNTU},\
	$(if $(filter $1, ${REL_RHEL}),${VER_REDHAT},\
	$(if $(filter $1, ${REL_SLES}),${VER_SLES},\
	$(error "Undefined target $1 in getOsVer function") \
	)))
endef

define ERROR_MESSAGE_TAG
TAG variable must be defined to name the docker image. 
The invocation should be:
TAG=NAME_OF_IMAGE make -f docker.mk

endef

ifndef TAG
$(error $(ERROR_MESSAGE_TAG))
endif

define WARNING_MESSAGE_PROXY
PROXY is not set.
If you wish to set the proxy for the docker build

PROXY=' --build-arg http_proxy=YOUR_PROXY_ADDRESS ' TAG=NAME_OF_IMAGE make -f docker.mk


endef

ifndef PROXY
$(warning $(WARNING_MESSAGE_PROXY))
endif

.PHONY: help
help:
	@echo Possible invocations are
	@echo TAG=<TAG_CONTAIER>  PROXY=' <PROXY_SETTINGS>' make -f docker.mk sles-15.3
	@echo TAG=<TAG_CONTAIER>  PROXY=' <PROXY_SETTINGS>' make -f docker.mk redhat-9
	@echo TAG=<TAG_CONTAIER>  PROXY=' <PROXY_SETTINGS>' make -f docker.mk ubuntu-24-04
	@echo TAG=<TAG_CONTAIER>  PROXY=' <PROXY_SETTINGS>' make -f docker.mk 



${OS_TARGETS}: 
	@DOCKER_BUILDKIT=${USE_BUILDKIT} docker build \
	--file $@/bldrun.Dockerfile ${PROXY}\
	--tag ""${TAG}:$(strip $(call getOsVer, $@))-$@_${ONEAPI_VER}"" .
