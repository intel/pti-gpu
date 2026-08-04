#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Clone and build protobuf (libprotobuf + protoc) from source into the build
# tree, so unitrace never depends on a system protobuf install. Installs a
# static, -fPIC build (no zlib dep) to <build_dir>/protobuf-install. Skips the
# work if protoc is already present there.

import glob
import os
import shutil
import sys
import subprocess

URL = "https://github.com/protocolbuffers/protobuf.git"


def _installed(install):
    protoc = os.path.join(install, "bin", "protoc.exe" if os.name == "nt" else "protoc")
    have_lib = bool(glob.glob(os.path.join(install, "lib*", "libprotobuf.*")) or
                    glob.glob(os.path.join(install, "lib*", "*protobuf*.lib")))
    return os.path.exists(protoc) and have_lib


def main():
    if len(sys.argv) < 3:
        print("Usage: python get_protobuf.py <build_dir> <tag> [generator] [make_program] [build_type]")
        return 1

    build_dir = sys.argv[1]
    tag = sys.argv[2]
    # Match the parent's generator; on Windows this keeps Ninja
    generator = sys.argv[3] if len(sys.argv) > 3 else ""
    make_program = sys.argv[4] if len(sys.argv) > 4 else ""
    parent_build_type = sys.argv[5] if len(sys.argv) > 5 else ""
    src = os.path.join(build_dir, "protobuf")
    bld = os.path.join(src, "build")
    install = os.path.join(build_dir, "protobuf-install")

    # Skip only if a COMPLETE install (protoc + lib) is already present.
    if _installed(install):
        return 0
    # Otherwise clear any partial install so this run starts clean.
    shutil.rmtree(install, ignore_errors=True)

    if not os.path.exists(src):
        if subprocess.call(["git", "clone", "--depth", "1", "--branch", tag, URL, src]) != 0:
            return 1

    cfg = ["cmake", "-S", src, "-B", bld]
    if generator:
        cfg += ["-G", generator]
        if make_program:
            cfg += ["-DCMAKE_MAKE_PROGRAM=" + make_program]
    # Follow the parent's build type so the MSVC runtime matches; default Debug on
    # Windows (unitrace's no-build-type default is /MDd), Release elsewhere.
    build_type = parent_build_type or ("Debug" if os.name == "nt" else "Release")
    cfg += [
        "-Wno-dev",
        "-DCMAKE_BUILD_TYPE=" + build_type,
        "-Dprotobuf_BUILD_TESTS=OFF",
        "-Dprotobuf_BUILD_SHARED_LIBS=OFF",
        "-Dprotobuf_WITH_ZLIB=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DCMAKE_INSTALL_PREFIX=" + install,
    ]
    if os.name == "nt":
        # Dynamic CRT (not protobuf's default /MT) so the lib links into unitrace.
        cfg += ["-Dprotobuf_MSVC_STATIC_RUNTIME=OFF", "-Dprotobuf_DEBUG_POSTFIX="]
        # Build version.rc without passing compiler flags (/bigobj etc.) to
        # rc.exe, which rejects them (RC1106). Redefine the RC command to drop
        # <FLAGS>, keeping only <DEFINES>. protobuf does this itself but only for
        # cl.exe, not icx.
        cfg += ["-DCMAKE_RC_COMPILE_OBJECT="
                "<CMAKE_RC_COMPILER> /l0x409 <DEFINES> /fo<OBJECT> <SOURCE>"]

    # protobuf is third-party; we don't fix its compiler warnings. (-w on
    # GCC/Clang, /w on MSVC.)
    nowarn = "/w" if os.name == "nt" else "-w"
    cfg += ["-DCMAKE_C_FLAGS=" + nowarn, "-DCMAKE_CXX_FLAGS=" + nowarn]
    if subprocess.call(cfg) != 0:
        return 1

    jobs = str(os.cpu_count() or 1)
    if subprocess.call(["cmake", "--build", bld, "--target", "install", "-j", jobs]) != 0:
        return 1

    # Confirm the install is actually complete before reporting success.
    return 0 if _installed(install) else 1


if __name__ == "__main__":
    sys.exit(main())
