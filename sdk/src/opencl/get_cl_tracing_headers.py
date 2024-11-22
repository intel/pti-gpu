import sys
import os

import build_utils

url = "https://github.com/intel/compute-runtime.git"
# commit = "ca7d47598a63959d42750c62a2981e08ffa392a1"
commit = "e16f47e375e4324dae07aadbfe953002a1c45195"


def postprocess(file_path):
    input = open(file_path, "rt")
    output = open(file_path + ".bk", "wt")

    for line in input.readlines():
        if line.find('#include "opencl/source/tracing/tracing_types.h"') >= 0:
            items = line.split("opencl/source/tracing/")
            assert len(items) == 2
            line = items[0] + items[1]
        output.write(line)

    input.close()
    output.close()

    os.remove(file_path)
    os.rename(file_path + ".bk", file_path)


def main():
    if len(sys.argv) < 3:
        print("Usage: python get_ocl_tracing_headers.py <include_path> <build_path>")
        return

    dst_path = sys.argv[1]
    if not os.path.exists(dst_path):
        os.mkdir(dst_path)
    dst_path = os.path.join(dst_path, "CL")
    if not os.path.exists(dst_path):
        os.mkdir(dst_path)

    clone_path = sys.argv[2]
    clone_path = os.path.join(clone_path, "compute-runtime")
    build_utils.clone(url, commit, clone_path)

    src_path = os.path.join(clone_path, "opencl")
    src_path = os.path.join(src_path, "source")
    src_path = os.path.join(src_path, "tracing")

    build_utils.copy(src_path, dst_path, ["tracing_api.h", "tracing_types.h"])

    postprocess(os.path.join(dst_path, "tracing_api.h"))

    src_path = os.path.join(clone_path, "opencl")
    src_path = os.path.join(src_path, "extensions")
    src_path = os.path.join(src_path, "public")

    build_utils.copy(src_path, dst_path, ["cl_ext_private.h"])


if __name__ == "__main__":
    main()
