#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

add_library(foomul SHARED foomul.cc)
target_link_libraries(foomul PRIVATE ittapi::ittnotify)
set_property(TARGET foomul PROPERTY POSITION_INDEPENDENT_CODE ON)
