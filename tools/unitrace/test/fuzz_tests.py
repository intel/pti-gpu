#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# The fuzz build step of test_unitrace.py's cpp-test group. The fuzz tests are
# the [fuzz_*] blocks of tests.txt; each runs test/fuzz/replay.py, which
# replays one target's seed corpus and regression inputs.
#
# The C++ targets include the unitrace sources, so they build with unitrace
# itself, configured with clang and -DUNITRACE_BUILD_FUZZ=ON in
# <unitrace build>/test/fuzz_build, with the same BUILD_WITH_* switches as the
# unitrace under test, except BUILD_WITH_OMP and BUILD_WITH_PERFETTO, which are
# always off, and with local clones of the repositories the main build fetched
# (headers, ittapi), so it does not download them again. The clang is the one on
# PATH or, in the oneAPI CI images, the one oneAPI ships. The fuzz tests need no
# GPU. Their C++ targets are skipped off Linux or without a clang with libFuzzer,
# the Python targets without atheris.

import os
import platform
import re
import shutil
import subprocess
import tempfile

import test_layout

_HERE = os.path.dirname(os.path.abspath(__file__))
UNITRACE_SOURCE_DIR = os.path.normpath(os.path.join(_HERE, ".."))

YELLOW = "\033[33m"
RESET = "\033[0m"


# The sanitizers the fuzz targets build with (test/fuzz/cpp/CMakeLists.txt).
SANITIZERS = "-fsanitize=fuzzer,address,undefined"


def _candidate_compilers():
    """(C compiler, C++ compiler) pairs to try, in order.

    clang on PATH first. Then the clang that oneAPI ships, which the CI images load
    (setvars.sh sets CMPLR_ROOT) without putting clang on PATH: bin/compiler/ from
    oneAPI 2024 on, linux/bin-llvm/ before. Then oneAPI's own icx/icpx drivers, which
    are clang-based too.
    """
    pairs = [("clang", "clang++")]
    root = os.environ.get("CMPLR_ROOT", "")
    if root:
        for sub in ("bin/compiler", "linux/bin-llvm"):
            pairs.append((os.path.join(root, sub, "clang"), os.path.join(root, sub, "clang++")))
    pairs.append(("icx", "icpx"))
    return pairs


def _links_fuzz_target(cxx):
    """true if cxx builds and links a trivial fuzz target with SANITIZERS: some
    distributions ship clang without libFuzzer or the sanitizer runtimes."""
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "probe.cc")
        with open(src, "w") as f:
            f.write('#include <cstddef>\n#include <cstdint>\n'
                    'extern "C" int LLVMFuzzerTestOneInput(const uint8_t*, size_t) { return 0; }\n')
        probe = subprocess.run([cxx, SANITIZERS, src, "-o", os.path.join(tmp, "probe")],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return probe.returncode == 0


def find_compilers():
    """The (C, C++) compilers to build the fuzz targets with: the first pair of
    _candidate_compilers() that exists and links a fuzz target. Returns
    (None, reason) if there is none."""
    found = False
    for cc, cxx in _candidate_compilers():
        if shutil.which(cc) is None or shutil.which(cxx) is None:
            continue
        found = True
        if _links_fuzz_target(cxx):
            return shutil.which(cc), shutil.which(cxx)
    if not found:
        return None, "no clang (PATH or oneAPI) found"
    return None, f"no clang links libFuzzer and the sanitizers ({SANITIZERS})"


def skip_reason():
    """Why the C++ fuzz targets cannot run here, or None if they can: they
    need Linux and a clang with libFuzzer (find_compilers()).
    """
    if platform.system() != "Linux":
        return f"not supported on {platform.system()}"
    cc, reason = find_compilers()
    return None if cc else reason


def _build_switches(unitrace_build_dir):
    """The -DBUILD_WITH_*=<value> options of the unitrace build under test, and the
    dependency paths it found (-D<Name>_LIBRARY, -D<Name>_INCLUDE_DIR).

    unitrace_build_dir: the build directory (None: the default one); its
    CMakeCache.txt is read. The fuzz build gets the same switches, so it compiles the
    same features and does not look for libraries the main build was configured
    without. Returns [] if there is no cache.
    """
    cache = os.path.join(test_layout.unitrace_build_dir(unitrace_build_dir), "CMakeCache.txt")
    switches = []
    if os.path.isfile(cache):
        with open(cache) as f:
            for line in f:
                m = re.match(r"(BUILD_WITH_[A-Z0-9_]+):[A-Z]+=(.*)$", line.strip())
                if m:
                    switches.append(f"-D{m.group(1)}={m.group(2)}")
                    continue
                # Where the main build found a dependency (e.g. Xptifw_LIBRARY and
                # Xptifw_INCLUDE_DIR given on its command line): the same, not looked up again.
                m = re.match(r"([A-Za-z0-9]+_(?:LIBRARY|INCLUDE_DIR)):(FILEPATH|PATH|STRING)=(.+)$", line.strip())
                if m and not m.group(3).endswith("-NOTFOUND"):
                    switches.append(f"-D{m.group(1)}:{m.group(2)}={m.group(3)}")
    return switches


def _reuse_header_clones(unitrace_build_dir, build_dir):
    """Clones, from the build under test, the repositories its configure and build
    fetched from GitHub (headers, ittapi) into the fuzz build, so the fetch scripts
    find them (build_utils.clone() skips a clone at the pinned commit) and do not go
    to the network again. A local clone shares the objects and checks out the same
    commit, but has its own work tree, which get_itt.py builds in. A repository the
    main build does not have (system headers used instead) is fetched as usual.
    """
    parent = test_layout.unitrace_build_dir(unitrace_build_dir)
    for name in ("level-zero", "intel-graphics-compiler", "compute-runtime", "OpenCL-Headers",
                 "ittapi"):
        src = os.path.join(parent, name)
        dst = os.path.join(build_dir, name)
        if os.path.isdir(os.path.join(src, ".git")) and not os.path.lexists(dst):
            os.makedirs(build_dir, exist_ok=True)
            subprocess.run(["git", "clone", "--quiet", src, dst], check=True)


def build(unitrace_build_dir=None):
    """Configures and builds the fuzz targets, part of the cpp-test build step.

    unitrace_build_dir: the unitrace build under test (None: the default one). The
    targets build with unitrace itself, with find_compilers()' clang and
    -DUNITRACE_BUILD_FUZZ=ON (and -DBUILD_WITH_OMP=0 -DBUILD_WITH_PERFETTO=OFF), into
    <unitrace build>/test/fuzz_build; the build is incremental. Returns 0 on success
    or when the targets cannot be built here (a Skipped notice is printed), 1 if the build
    fails.
    """
    reason = skip_reason()
    if reason:
        print(f"{YELLOW}fuzz build: Skipped ({reason}){RESET}")
        return 0
    cc, cxx = find_compilers()
    print(f"Fuzz build compiler: {cxx}")
    build_dir = test_layout.fuzz_build_dir(unitrace_build_dir)
    configure = ["cmake", "-S", UNITRACE_SOURCE_DIR, "-B", build_dir, "-G", "Ninja",
                 "-DUNITRACE_BUILD_FUZZ=ON",
                 f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}"]
    configure += _build_switches(unitrace_build_dir)
    # Without the OpenMP collector, whatever the build under test has: CMake's
    # FindOpenMP depends on the compiler (it fails for oneAPI's bare clang), and no
    # fuzzed parser is in it. Given last, so it overrides the forwarded switch.
    configure.append("-DBUILD_WITH_OMP=0")
    # Without Perfetto output either: no fuzzed parser is in it, and its setup
    # downloads the trace proto and builds protobuf a second time.
    configure.append("-DBUILD_WITH_PERFETTO=OFF")
    _reuse_header_clones(unitrace_build_dir, build_dir)
    try:
        print("Fuzz build started ... ")
        subprocess.run(configure, check=True)
        subprocess.run(["cmake", "--build", build_dir, "--target", "unitrace_fuzz"], check=True)
        print("Fuzz build completed ...")
        return 0
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] during fuzz build: {e}")
        return 1
