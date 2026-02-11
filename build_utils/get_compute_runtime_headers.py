import sys
import os

import build_utils

url = "https://github.com/intel/compute-runtime.git"
commit = "aa5ab7a8288f96f1bed187cec0d359bd65354ef3"

def postprocess(file_path):
    """Post-process header file: remove non-local tracing header includes.

    The compute-runtime repository uses internal include paths that need
    to be adjusted for external inclusion.
    """
    try:
        with open(file_path, "rt") as input_file:
            lines = input_file.readlines()

        with open(file_path + ".bk", "wt") as output_file:
            for line in lines:
                if line.find("#include \"opencl/source/tracing/tracing_types.h\"") >= 0:
                    items = line.split("opencl/source/tracing/")
                    if len(items) == 2:
                        line = items[0] + items[1]
                output_file.write(line)

        # Replace original with processed version
        os.remove(file_path)
        os.rename(file_path + ".bk", file_path)
    except IOError as e:
        raise IOError(f"Failed to post-process {file_path}: {e}")

def main():
    if len(sys.argv) < 3:
        print("Usage: python get_compute_runtime_headers.py <include_path> <build_path>")
        return 1

    dst_path = sys.argv[1]
    if not os.path.exists(dst_path):
        try:
            os.makedirs(dst_path)
        except OSError as e:
            print(f"ERROR: Failed to create destination directory {dst_path}: {e}")
            return 1

    build_path = sys.argv[2]

    try:
        # Clone compute-runtime
        clone_path = os.path.join(build_path, "compute-runtime")
        print(f"Cloning compute-runtime to {clone_path}...")
        build_utils.clone(url, commit, clone_path)

        # Copy OpenCL tracing headers
        cl_dst_path = os.path.join(dst_path, "CL")
        if not os.path.exists(cl_dst_path):
            os.makedirs(cl_dst_path)

        src_path = os.path.join(clone_path, "opencl", "source", "tracing")
        print(f"Copying OpenCL tracing headers from {src_path}...")
        build_utils.copy(src_path, cl_dst_path, ["tracing_api.h", "tracing_types.h"])

        postprocess(os.path.join(cl_dst_path, "tracing_api.h"))

        src_path = os.path.join(clone_path, "opencl", "extensions", "public")
        print(f"Copying OpenCL extension headers from {src_path}...")
        build_utils.copy(src_path, cl_dst_path, ["cl_ext_private.h"])

        # Copy Level Zero driver_experimental headers (zex_graph.h)
        l0_dst_path = os.path.join(dst_path, "level_zero")
        if not os.path.exists(l0_dst_path):
            os.makedirs(l0_dst_path)

        # Copy ze_stypes.h and ze_intel_gpu.h (required by extension headers)
        src_path = os.path.join(clone_path, "level_zero", "include", "level_zero")
        print(f"Copying Level Zero headers from {src_path}...")
        build_utils.copy(src_path, l0_dst_path, ["ze_stypes.h", "ze_intel_gpu.h"])

        l0_exp_dst_path = os.path.join(l0_dst_path, "driver_experimental")
        if not os.path.exists(l0_exp_dst_path):
            os.makedirs(l0_exp_dst_path)

        src_path = os.path.join(clone_path, "level_zero", "include", "level_zero", "driver_experimental")
        print(f"Copying Level Zero driver_experimental headers from {src_path}...")
        # Copy all zex_*.h headers from driver_experimental
        zex_headers = [f for f in os.listdir(src_path) if f.startswith("zex_") and f.endswith(".h")]
        if zex_headers:
            build_utils.copy(src_path, l0_exp_dst_path, zex_headers)

        print(f"SUCCESS: All headers copied to {dst_path}")
        return 0

    except Exception as e:
        print(f"ERROR: Failed to extract headers: {e}")
        return 1

if __name__ == "__main__":
  main()