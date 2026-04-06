USE_BUILDKIT := 1

REL_UBUNTU_24:= ubuntu-24-04
#
# Observation 4/2/26 is that SERVER is working with Ubuntu_25_10 container
# But it is aimed at CLIENT
#
REL_UBUNTU_25_10:= ubuntu-25-10
REL_RHEL:= redhat-10
REL_SLES:= sles-15
REL_ROCKY:= rocky-8
SERVER_TARGETS:= ${REL_SLES} ${REL_UBUNTU_24} ${REL_ROCKY} ${REL_RHEL}
CLIENT_TARGETS:= ${REL_UBUNTU_25_10}
.PHONY:${SERVER_TARGETS} ${CLIENT_TARGETS}

ONEAPI_VER=2025.3.0

targets: ${SERVER_TARGETS} ${CLIENT_TARGETS}
	@echo BUILT ${SERVER_TARGETS} ${CLIENT_TARGETS}

OS_TARGETS_MIN:=$(foreach os, ${SERVER_TARGETS} ${CLIENT_TARGETS} ,  min_os_${os})

.PHONY: min_os_all
min_os_all: ${OS_TARGETS_MIN}

VER_UBUNTU_24:=06
VER_UBUNTU_25_10:=06
VER_REDHAT:=06
VER_SLES:=06
VER_ROCKY:=06
define getOsVer
	$(if $(filter $1, ${REL_UBUNTU_24}),${VER_UBUNTU_24},\
	$(if $(filter $1, ${REL_UBUNTU_25_10}),${VER_UBUNTU_25_10},\
	$(if $(filter $1, ${REL_RHEL}),${VER_REDHAT},\
	$(if $(filter $1, ${REL_SLES}),${VER_SLES},\
	$(if $(filter $1, ${REL_ROCKY}),${VER_ROCKY},\
	$(error "Undefined target $1 in getOsVer function") \
	)))))
endef

define ERROR_MESSAGE_PTI_CONTAINER
PTI_CONTAINER_NAME variable must be defined.
It is the name for the container we are building for PTI.
The invocation should be:
PTI_CONTAINER_NAME=NAME make -f docker.mk

endef

ifndef PTI_CONTAINER_NAME
$(error $(ERROR_MESSAGE_PTI_CONTAINER))
endif

define WARNING_MESSAGE_PROXY
PROXY is not set.
If you wish to set the proxy for the docker build

PROXY='http_proxy=YOUR_PROXY_ADDRESS,https_proxy=YOUR_PROXY_ADDRESS ' PTI_CONTAINER_NAME=NAME_OF_IMAGE make -f docker.mk


endef

ifndef PROXY
$(warning $(WARNING_MESSAGE_PROXY))
endif

.PHONY: help
help:
	@echo Possible invocations are
	@echo PTI_CONTAINER_NAME=<NAME> PROXY='<http_proxy=X,https_proxy=Y>' make -f docker.mk sles-15
	@echo PTI_CONTAINER_NAME=<NAME> PROXY='<http_proxy=X,https_proxy=Y>' make -f docker.mk redhat-10
	@echo PTI_CONTAINER_NAME=<NAME> PROXY='<http_proxy=X,https_proxy=Y>' make -f docker.mk ubuntu-24-04
	@echo PTI_CONTAINER_NAME=<NAME> PROXY='<http_proxy=X,https_proxy=Y>' make -f docker.mk ubuntu-25-10
	@echo PTI_CONTAINER_NAME=<NAME>  PROXY='<PROXY_SETTINGS>' make -f docker.mk


PARSED_PROXY:=$(shell IFS=','; for i in  $${PROXY}; do echo -n " --build-arg " $$i; done)

# Rule to build the minimal OS image for each target.
# This builds a Docker image using the min_os.Dockerfile for the given OS.
# This allows invocations like this one:
# 	PROXY='http_proxy=XX,https_proxy=YY' PTI_CONTAINER_NAME=pti_container make -f docker.mk  min_os_sles-15
min_os_%:
	@echo Building $@
	@DOCKER_BUILDKIT=${USE_BUILDKIT} docker build \
	--file $(patsubst min_os_%,%,$@)/min_os.Dockerfile ${PARSED_PROXY} \
	--tag ""${PTI_CONTAINER_NAME}_min_os:$(strip $(call getOsVer, $(patsubst min_os_%,%, $@)))-$(patsubst min_os_%,%, $@)"" .

# For each OS target, define a rule that depends on its minimal OS image.
# Then build the full image using bldrun.Dockerfile, passing the minimal image as a build argument.
# Also prints proxy settings and tags the resulting image with version and oneAPI version.
# This allows invocations like this one:
#   PROXY='http_proxy=XX,https_proxy=YY' PTI_CONTAINER_NAME=pti_container make -f docker.mk  sles-15
$(foreach os,$(SERVER_TARGETS) $(CLIENT_TARGETS),$(eval $(os): min_os_$(os) ;\
	@echo "Building $(os) from min_os_$(os)" ; \
	echo "Proxy setting:" ${PARSED_PROXY}  ; \
	DOCKER_BUILDKIT=${USE_BUILDKIT} docker build \
	--file $(os)/bldrun.Dockerfile ${PARSED_PROXY} \
	--build-arg MIN_OS_CONTAINER=""${PTI_CONTAINER_NAME}_min_os:$(strip $(call getOsVer, $(os)))-$(os)"" \
	--tag ""${PTI_CONTAINER_NAME}:$(strip $(call getOsVer, $(os)))-$(os)_${ONEAPI_VER}"" . \
))
