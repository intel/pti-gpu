@echo off

rem Copyright 2024 Intel Corporation
rem SPDX-License-Identifier: MIT

rem Permission is hereby granted, free of charge, to any person obtaining a copy
rem of this software and associated documentation files (the "Software"), to deal
rem in the Software without restriction, including without limitation the rights
rem to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
rem copies of the Software, and to permit persons to whom the Software is
rem furnished to do so, subject to the following conditions:
rem
rem The above copyright notice and this permission notice shall be included in all
rem copies or substantial portions of the Software.
rem
rem THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
rem IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
rem FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
rem AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
rem LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
rem OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
rem SOFTWARE.


setlocal EnableDelayedExpansion

set "__this_dir_path=%~dp0"
set "__this_dir_path=%__this_dir_path:~0,-1%"

set "PTI_ROOT=%__this_dir_path%\.."

endlocal & set "PTI_ROOT=%PTI_ROOT%"

set "PATH=%PTI_ROOT%\bin;%PATH%"
set "CPATH=%PTI_ROOT%\include;%CPATH%"
set "CMAKE_PREFIX_PATH=%PTI_ROOT%\lib\cmake\pti;%CMAKE_PREFIX_PATH%"
set "LIB=%PTI_ROOT%\lib;%LIB%"
set "LIBRARY_PATH=%PTI_ROOT%\lib;%LIBRARY_PATH%"
set "INCLUDE=%PTI_ROOT%\include;%INCLUDE%"

set "PTI_ROOT="
set "__this_dir_path="
