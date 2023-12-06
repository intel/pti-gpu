import sys
import os

import build_utils

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_gtpin_headers_legacy.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "GTPIN")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  
  build_path = sys.argv[2]
  if sys.platform == 'win32':
    gtpin_package = "external-gtpin-2.19-win.zip"
    download_link = "https://downloadmirror.intel.com/686382/"
  else:
    gtpin_package = "external-gtpin-2.19-linux.tar.xz"
    download_link = "https://downloadmirror.intel.com/686383/"
  build_utils.download(download_link + gtpin_package, build_path)
  arch_file = os.path.join(build_path, gtpin_package)
  build_utils.unpack(arch_file, build_path)

  src_path = os.path.join(build_path, "Profilers")
  src_path = os.path.join(src_path, "Include")

  build_utils.copy(src_path, dst_path,
    ["callbacks.h",
     "client_knob.h",
     "clientdb.h",
     "ged_ops.h",
     "gtpin.h",
     "gtpin.hpp",
     "gtpintool_types.h",
     "init.h",
     "kernel.h",
     "send_exec_semantics.h"])

  dst_path_ged = os.path.join(dst_path, "ged")
  if (not os.path.exists(dst_path_ged)):
    os.mkdir(dst_path_ged)
  dst_path_ged = os.path.join(dst_path_ged, "intel64")
  if (not os.path.exists(dst_path_ged)):
    os.mkdir(dst_path_ged)

  src_path_ged = os.path.join(src_path, "ged")
  src_path_ged = os.path.join(src_path_ged, "intel64")

  build_utils.copy(src_path_ged, dst_path_ged,
    ["ged_basic_types.h",
     "ged_enumerations.h",
     "ged_enum_types.h",
     "ged.h",
     "ged_ins_field.h"])

  dst_path_api = os.path.join(dst_path, "api")
  if (not os.path.exists(dst_path_api)):
    os.mkdir(dst_path_api)

  src_path_api = os.path.join(src_path, "api")

  build_utils.copy(src_path_api, dst_path_api,
    ["gt_knob.h",
     "gt_knob_defs.h",
     "igt_knob_arg.h",
     "igt_knob_registry.h",
     "gt_basic_defs.h",
     "igt_core.h",
     "gt_gpu_defs.h",
     "gt_basic_utils.h"])

if __name__ == "__main__":
  main()
