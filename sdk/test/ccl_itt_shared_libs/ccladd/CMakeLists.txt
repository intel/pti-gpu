#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

add_library(ccladd SHARED ccladd.cc)
target_link_libraries(ccladd PRIVATE ittapi::ittnotify)
set_property(TARGET ccladd PROPERTY POSITION_INDEPENDENT_CODE ON)
