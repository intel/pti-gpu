//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_HOST_UTILS_H_
#define PTI_UTILS_HOST_UTILS_H_

#include <string>
#ifdef _WIN32
#include <windows.h>
#else /* _WIN32 */
#include <unistd.h>
#endif /* _WIN32 */

static inline std::string GetHostName(void) {
  char hname[256];
#ifdef _WIN32
 static_assert(256 >= MAX_COMPUTERNAME_LENGTH + 1, "Buffer too small to fit hostname");
  DWORD size = sizeof(hname);
  GetComputerNameA(hname, &size);
#else  /* _WIN32 */
  gethostname(hname, sizeof(hname));
#endif /* _WIN32 */
  hname[255] = 0;
  return hname;
}

#endif // PTI_UTILS_HOST_UTILS_H_
