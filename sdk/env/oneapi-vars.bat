@echo off
REM
REM Copyright (c) 2024 Intel Corporation
REM
REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM     http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by applicable law or agreed to in writing, software
REM distributed under the License is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM See the License for the specific language governing permissions and
REM limitations under the License.
REM

if not defined SETVARS_CALL (
    echo:
    echo :: ERROR: This script must be executed by setvars.bat.
    echo:   Try '[install-dir]\setvars.bat --help' for help.
    echo:
    exit /b 255
)

if not defined ONEAPI_ROOT (
    echo:
    echo :: ERROR: This script requires that the ONEAPI_ROOT env variable is set."
    echo:   Try '[install-dir]\setvars.bat --help' for help.
    echo:
    exit /b 254
)

set "PTI_ROOT=%ONEAPI_ROOT%"
set "CMAKE_PREFIX_PATH=%PTI_ROOT%\lib\cmake\pti;%CMAKE_PREFIX_PATH%"
set "CPATH=%PTI_ROOT%\include;%CPATH%"

set "Pti_DIR=%PTI_ROOT%\lib\cmake\pti"

exit /B 0
