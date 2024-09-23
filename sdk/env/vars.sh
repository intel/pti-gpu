#!/bin/sh
# shellcheck shell=sh

# Copyright 2024 Intel Corporation
# SPDX-License-Identifier: MIT

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


# ############################################################################

# Get absolute path to this script.
# Uses `readlink` to remove links and `pwd -P` to turn into an absolute path.

# Usage:
#   script_dir=$(get_script_path "$script_rel_path")
#
# Inputs:
#   script/relative/pathname/scriptname
#
# Outputs:
#   /script/absolute/pathname

# executing function in a *subshell* to localize vars and effects on `cd`

# The sequence `builtin cd` needs to be used with zsh instead of `command cd`
# to remove alias/function redefinitions of the `cd` command. This is because
# `command` only works with external commands in the zsh shell. Unfortunately,
# `builtin` is not recognized by `dash` and fails. Thus it is necessary to
# create two branches in the function to insure that an existing redefinition
# of `cd` (alias or function) does not interfere.

get_script_path() (
  script="$1"
  while [ -L "$script" ] ; do
    script_dir=$(command dirname -- "$script")
    # see also: https://superuser.com/a/1574553/229501
    if [ -n "${ZSH_VERSION:-}" ] ; then
      script_dir=$(builtin cd "$script_dir" && command pwd -P)
    else
      script_dir=$(command cd "$script_dir" && command pwd -P)
    fi
    script="$(readlink "$script")"
    case $script in
      (/*) ;;
       (*) script="$script_dir/$script" ;;
    esac
  done
  script_dir=$(command dirname -- "$script")
  if [ -n "${ZSH_VERSION:-}" ] ; then
    script_dir=$(builtin cd "$script_dir" && command pwd -P)
  else
    script_dir=$(command cd "$script_dir" && command pwd -P)
  fi
  printf "%s" "$script_dir"
)


# ############################################################################

# Determine if we are being executed or sourced. Need to detect being sourced
# within an executed script, which can happen on a CI system. We also must
# detect being sourced at a shell prompt (CLI). The setvars.sh script will
# always source this script, but this script can also be called directly.

# We are assuming we know the name of this script, which is a reasonable
# assumption. This script _must_ be named "vars.sh" or it will not work
# with the top-level setvars.sh script. Making this assumption simplifies
# the process of detecting if the script has been sourced or executed. It
# also simplifies the process of detecting the location of this script.

# Using `readlink` to remove possible symlinks in the name of the script.
# Also, "ps -o comm=" is limited to a 15 character result, but it works
# fine here, because we are only looking for the name of this script or the
# name of the execution shell, both always fit into fifteen characters.

_vars_get_proc_name() {
  if [ -n "${ZSH_VERSION:-}" ] ; then
    script="$(ps -p "$$" -o comm=)"
  else
    script="$1"
    while [ -L "$script" ] ; do
      script="$(readlink "$script")"
    done
  fi
  basename -- "$script"
}

_vars_this_script_name="vars.sh"
if [ "$_vars_this_script_name" = "$(_vars_get_proc_name "$0")" ] ; then
  echo "   ERROR: Incorrect usage: this script must be sourced."
  echo "   Usage: . path/to/${_vars_this_script_name}"
  return 255 2>/dev/null || exit 255
fi


# ############################################################################

# Prepend path segment(s) to path-like env vars (PATH, CPATH, etc.).

# prepend_path() avoids dangling ":" that affects some env vars (PATH and CPATH)
# prepend_manpath() includes dangling ":" needed by MANPATH.
# PATH > https://www.gnu.org/software/libc/manual/html_node/Standard-Environment.html
# MANPATH > https://manpages.debian.org/stretch/man-db/manpath.1.en.html

# Usage:
#   env_var=$(prepend_path "$prepend_to_var" "$existing_env_var")
#   export env_var
#
#   env_var=$(prepend_manpath "$prepend_to_var" "$existing_env_var")
#   export env_var
#
# Inputs:
#   $1 == path segment to be prepended to $2
#   $2 == value of existing path-like environment variable

prepend_path() (
  path_to_add="$1"
  path_is_now="$2"

  if [ "" = "${path_is_now}" ] ; then   # avoid dangling ":"
    printf "%s" "${path_to_add}"
  else
    printf "%s" "${path_to_add}:${path_is_now}"
  fi
)

prepend_manpath() (
  path_to_add="$1"
  path_is_now="$2"

  if [ "" = "${path_is_now}" ] ; then   # include dangling ":"
    printf "%s" "${path_to_add}:"
  else
    printf "%s" "${path_to_add}:${path_is_now}"
  fi
)


# ############################################################################

# Extract the name and location of this sourced script.

# Generally, "ps -o comm=" is limited to a 15 character result, but it works
# fine for this usage, because we are primarily interested in finding the name
# of the execution shell, not the name of any calling script.

vars_script_name=""
vars_script_shell="$(ps -p "$$" -o comm=)"
# ${var:-} needed to pass "set -eu" checks
# see https://unix.stackexchange.com/a/381465/103967
# see https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06_02
if [ -n "${ZSH_VERSION:-}" ] && [ -n "${ZSH_EVAL_CONTEXT:-}" ] ; then     # zsh 5.x and later
  # shellcheck disable=SC2249,SC2296
  case $ZSH_EVAL_CONTEXT in (*:file*) vars_script_name="${(%):-%x}" ;; esac ;
elif [ -n "${KSH_VERSION:-}" ] ; then                                     # ksh, mksh or lksh
  if [ "$(set | grep -Fq "KSH_VERSION=.sh.version" ; echo $?)" -eq 0 ] ; then # ksh
    # shellcheck disable=SC2296
    vars_script_name="${.sh.file}" ;
  else # mksh or lksh or [lm]ksh masquerading as ksh or sh
    # shellcheck disable=SC2296
    # force [lm]ksh to issue error msg; which contains this script's path/filename, e.g.:
    # mksh: /home/ubuntu/intel/oneapi/vars.sh[137]: ${.sh.file}: bad substitution
    vars_script_name="$( (echo "${.sh.file}") 2>&1 )" || : ;
    vars_script_name="$(expr "${vars_script_name:-}" : '^.*sh: \(.*\)\[[0-9]*\]:')" ;
  fi
elif [ -n "${BASH_VERSION:-}" ] ; then        # bash
  # shellcheck disable=SC2128,SC3028
  (return 0 2>/dev/null) && vars_script_name="${BASH_SOURCE}" ;
elif [ "dash" = "$vars_script_shell" ] ; then # dash
  # shellcheck disable=SC2296
  # force dash to issue error msg; which contains this script's rel/path/filename, e.g.:
  # dash: 146: /home/ubuntu/intel/oneapi/vars.sh: Bad substitution
  vars_script_name="$( (echo "${.sh.file}") 2>&1 )" || : ;
  vars_script_name="$(expr "${vars_script_name:-}" : '^.*dash: [0-9]*: \(.*\):')" ;
elif [ "sh" = "$vars_script_shell" ] ; then   # could be dash masquerading as /bin/sh
  # shellcheck disable=SC2296
  # force a shell error msg; which should contain this script's path/filename
  # sample error msg shown; assume this file is named "vars.sh"; as required by setvars.sh
  vars_script_name="$( (echo "${.sh.file}") 2>&1 )" || : ;
  if [ "$(printf "%s" "$vars_script_name" | grep -Eq "sh: [0-9]+: .*vars\.sh: " ; echo $?)" -eq 0 ] ; then # dash as sh
    # sh: 155: /home/ubuntu/intel/oneapi/vars.sh: Bad substitution
    vars_script_name="$(expr "${vars_script_name:-}" : '^.*sh: [0-9]*: \(.*\):')" ;
  fi
else  # unrecognized shell or dash being sourced from within a user's script
  # shellcheck disable=SC2296
  # force a shell error msg; which should contain this script's path/filename
  # sample error msg shown; assume this file is named "vars.sh"; as required by setvars.sh
  vars_script_name="$( (echo "${.sh.file}") 2>&1 )" || : ;
  if [ "$(printf "%s" "$vars_script_name" | grep -Eq "^.+: [0-9]+: .*vars\.sh: " ; echo $?)" -eq 0 ] ; then # dash
    # .*: 164: intel/oneapi/vars.sh: Bad substitution
    vars_script_name="$(expr "${vars_script_name:-}" : '^.*: [0-9]*: \(.*\):')" ;
  else
    vars_script_name="" ;
  fi
fi

if [ "" = "$vars_script_name" ] ; then
  >&2 echo "   ERROR: Unable to proceed: possible causes listed below."
  >&2 echo "   This script must be sourced. Did you execute or source this script?" ;
  >&2 echo "   Unrecognized/unsupported shell (supported: bash, zsh, ksh, m/lksh, dash)." ;
  >&2 echo "   May fail in dash if you rename this script (assumes \"vars.sh\")." ;
  >&2 echo "   Can be caused by sourcing from ZSH version 4.x or older." ;
  return 255 2>/dev/null || exit 255
fi


my_script_name=$(basename -- "${vars_script_name:-}")
my_script_path=$(get_script_path "${vars_script_name:-}")
component_root=$(dirname -- "${my_script_path}")

# Add component include folder to CPATH, using prepend_path() function.
CPATH=$(prepend_path "${component_root}/include" "${CPATH:-}") ; export CPATH

CMAKE_PREFIX_PATH=$(prepend_path "${component_root}/lib/cmake/pti" "${CMAKE_PREFIX_PATH:-}") ; export CMAKE_PREFIX_PATH

LD_LIBRARY_PATH=$(prepend_path "${component_root}/lib" "${LD_LIBRARY_PATH:-}") ; export LD_LIBRARY_PATH

LIBRARY_PATH=$(prepend_path "${component_root}/lib" "${LIBRARY_PATH:-}") ; export LIBRARY_PATH

#
# This variable allows cmake to find PTI in conda environments to
# bypass the overwritting of the LD_LIBRARY_PATH
#
Pti_DIR=${component_root}/lib/cmake/pti; export Pti_DIR
