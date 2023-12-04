
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNICONTROL_H
#define PTI_TOOLS_UNITRACE_UNICONTROL_H

#include "utils.h"
#include <iostream>

extern char **environ;

class UniController{
  public:
    static bool IsCollectionEnabled(void) {
      if (conditional_collection_) {
        if (itt_paused_) {
          return false;
        }
        if (environ != nullptr) {
          char *env;
          char *value = nullptr;
          constexpr int len = sizeof("PTI_ENABLE_COLLECTION") - 1;	// do not count trailing '\0' 
          char **cursor = environ;
          // PTI_ENABLE_COLLECTION is likely at the end if it is set
          while (*cursor) {
            cursor++;	
          }
          cursor--;
          for (; (cursor != environ - 1) && ((env = *cursor) != nullptr); cursor--) {
            if ((env[0] == 'P') && (env[1] == 'T') && (env[2] == 'I') && (strncmp(env + 3, "_ENABLE_COLLECTION", len - 3) == 0) && (env[len] == '=')) {
              value = (env + len + 1); 
              break;
            }
          }

          if ((value == nullptr) || (*value == '0')) {
            return false;
          }
        }
      }
      return true;
    }
    static void IttPause(void) {
      itt_paused_ = true;
      utils::SetEnv("PTI_ENABLE_COLLECTION", "0");
    }
    static void IttResume(void) {
      itt_paused_ = false;
      utils::SetEnv("PTI_ENABLE_COLLECTION", "1");
    }
  private:
    inline static bool conditional_collection_ = (utils::GetEnv("UNITRACE_ConditionalCollection") == "1") ? true : false;
    inline static bool itt_paused_ = false;
};
    
#endif // PTI_TOOLS_UNITRACE_UNICONTROL_H
