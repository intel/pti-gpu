//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file tool_factory.cpp
 * @brief Contains the implementation of the ToolFactory class.
 *
 * This file provides the implementation of the ToolFactory class, which is responsible for creating
 * tools and managing their lifecycle. It also includes the definition of the ToolFactory
 * constructor and the GetControl method.
 */

#include "tool_factory.hpp"

using namespace gtpin_prof;

ToolFactory::ToolFactory(const ControlBaseSPtr control) : m_control(control) {
  PTI_ASSERT(control != nullptr);
}

const ControlBaseSPtr ToolFactory::GetControl() { return m_control; }
