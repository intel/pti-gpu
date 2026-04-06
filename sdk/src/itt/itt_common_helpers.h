//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef ITT_COMMON_HELPERS_H_
#define ITT_COMMON_HELPERS_H_

/**
 * @internal
 * @file itt_common_helpers.h
 * @brief Common helper functions for ITT API implementations
 *
 * This file contains shared logic used by both itt_collector.cc and itt_adapter.cc
 * to avoid code duplication while maintaining consistent behavior.
 */
#include <ittnotify.h>
#include <ittnotify_config.h>

#include <cstdlib>
#include <cstring>

namespace itt_helpers {

/**
 * @internal
 * @brief Find existing string handle or create a new one
 *
 * This helper encapsulates the common logic for looking up a string handle
 * in the global list, or creating it if it doesn't exist.
 *
 * @param itt_global Pointer to the global ITT state structure
 * @param name The string name to find or create a handle for
 * @return __itt_string_handle* Pointer to the found or created handle, or nullptr if itt_global is
 * null
 */
static inline __itt_string_handle* FindOrCreateStringHandle(__itt_global* itt_global,
                                                            const char* name) {
  if (itt_global == nullptr) {
    return nullptr;
  }

  __itt_string_handle *h_tail = nullptr, *h = nullptr;

  __itt_mutex_lock(&(itt_global->mutex));
  for (h_tail = nullptr, h = itt_global->string_list; h != nullptr; h_tail = h, h = h->next) {
    if (h->strA != nullptr && !__itt_fstrcmp(h->strA, name)) break;
  }
  if (h == nullptr) {
    NEW_STRING_HANDLE_A(itt_global, h, h_tail, name);
  }
  __itt_mutex_unlock(&(itt_global->mutex));

  return h;
}

}  // namespace itt_helpers

#endif  // ITT_COMMON_HELPERS_H_
