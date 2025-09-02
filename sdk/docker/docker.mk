USE_BUILDKIT := 1

REL_UBUNTU_22:= ubuntu-22-04
REL_UBUNTU_24:= ubuntu-24-04
#
# TODO: 5/21/2025
# Can't build redhat-9 nor rocky-8 for PVC, there are missing
# libraries
#
REL_RHEL:= redhat-9
REL_SLES:= sles-15
REL_ROCKY:= rocky-8
OS_TARGETS:= ${REL_SLES} ${REL_UBUNTU_22} ${REL_UBUNTU_24}
.PHONY:${OS_TARGETS}

ONEAPI_VER=2025.2.0

targets: ${OS_TARGETS}
	@echo BUILT ${OS_TARGETS}

VER_UBUNTU_22:=03
VER_UBUNTU_24:=03
VER_REDHAT:=03
VER_SLES:=03
VER_ROCKY:=03
define getOsVer
	$(if $(filter $1, ${REL_UBUNTU_22}),${VER_UBUNTU_22},\
	$(if $(filter $1, ${REL_UBUNTU_24}),${VER_UBUNTU_24},\
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
	@echo PTI_CONTAINER_NAME=<NAME> PROXY='<http_proxy=X,https_proxy=Y>' make -f docker.mk redhat-9
	@echo PTI_CONTAINER_NAME=<NAME> PROXY='<http_proxy=X,https_proxy=Y>' make -f docker.mk ubuntu-22-04
	@echo PTI_CONTAINER_NAME=<NAME> PROXY='<http_proxy=X,https_proxy=Y>' make -f docker.mk ubuntu-24-04
	@echo PTI_CONTAINER_NAME=<NAME>  PROXY='<PROXY_SETTINGS>' make -f docker.mk 


PARSED_PROXY:=$(shell IFS=','; for i in  $${PROXY}; do echo -n " --build-arg " $$i; done)

OS_TARGETS_MIN:=$(foreach os, ${OS_TARGETS},  min_os_${os})
.PHONY: ${OS_TARGETS}

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
$(foreach os,$(OS_TARGETS),$(eval $(os): min_os_$(os) ;\
	@echo "Building $(os) from min_os_$(os)" ; \
	echo "Proxy setting:" ${PARSED_PROXY}  ; \
	DOCKER_BUILDKIT=${USE_BUILDKIT} docker build \
	--file $(os)/bldrun.Dockerfile ${PARSED_PROXY} \
	--build-arg MIN_OS_CONTAINER=""${PTI_CONTAINER_NAME}_min_os:$(strip $(call getOsVer, $(os)))-$(os)"" \
	--tag ""${PTI_CONTAINER_NAME}:$(strip $(call getOsVer, $(os)))-$(os)_${ONEAPI_VER}"" . \
))
