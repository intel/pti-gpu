#!/bin/sh
# shellcheck shell=sh

##===----------------------------------------------------------------------===##
#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# This file incorporates work covered by the following copyright and permission
# notice:
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
#
##===----------------------------------------------------------------------===##

if [ -z "${SETVARS_CALL:-}" ] ; then
  >&2 echo " "
  >&2 echo ":: ERROR: This script must be sourced by oneapi-vars.sh."
  >&2 echo "   Try 'source <install-dir>/oneapi-vars.sh --help' for help."
  >&2 echo " "
  return 255
fi

if [ -z "${ONEAPI_ROOT:-}" ] ; then
  >&2 echo " "
  >&2 echo ":: ERROR: This script requires that the ONEAPI_ROOT env variable is set."
  >&2 echo "   Try 'source <install-dir>\oneapi-vars.sh --help' for help."
  >&2 echo " "
  return 254
fi

# ############################################################################

CMAKE_PREFIX_PATH=$(prepend_path "${component_root}/lib/cmake/pti" "${CMAKE_PREFIX_PATH:-}") ; export CMAKE_PREFIX_PATH

C_INCLUDE_PATH=$(prepend_path "${component_root}/include" "${C_INCLUDE_PATH:-}") ; export C_INCLUDE_PATH
CPLUS_INCLUDE_PATH=$(prepend_path "${component_root}/include" "${CPLUS_INCLUDE_PATH:-}") ; export CPLUS_INCLUDE_PATH

Pti_DIR=${ONEAPI_ROOT}/lib/cmake/pti; export Pti_DIR


