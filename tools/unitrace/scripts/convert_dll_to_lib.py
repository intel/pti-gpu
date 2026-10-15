import os
import sys
import subprocess
import shutil
import re

def main():
  if len(sys.argv) < 3:
    print("Usage: python convert_dll_to_lib.py <path to the generated .lib file> <.dll file path>")
    return 1

  if os.environ.get('OS','') != 'Windows_NT':
    print("This script is only for Windows")
    return 1
  else:
    lib_path = sys.argv[1]
    dll_file = sys.argv[2]

    dumpbin = shutil.which("dumpbin")
    librarian = shutil.which("lib")
    if not dumpbin or not librarian:
      raise RuntimeError("dumpbin and lib must be available on PATH")
    if not os.path.isfile(dll_file) or not dll_file.lower().endswith(".dll"):
      raise FileNotFoundError(dll_file)

    if not os.path.exists(lib_path):
      os.mkdir(lib_path)

    lib_name = os.path.basename(dll_file)
    lib_name = os.path.splitext(lib_name)[0]

    def_file = os.path.join(lib_path, lib_name + ".def")
    lib_file = os.path.join(lib_path, lib_name + ".lib")

    exports = subprocess.check_output([dumpbin, "/exports", dll_file], text=True)
    symbols = re.findall(r"^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)",
                         exports, re.MULTILINE)
    if not symbols:
      raise RuntimeError(f"No exports found in {dll_file}")
    with open(def_file, "w") as output:
      output.write("EXPORTS\n" + "\n".join(symbols) + "\n")
    subprocess.check_call([librarian, "/def:" + def_file, "/out:" + lib_file, "/machine:x64"])
    if not os.path.isfile(lib_file):
      raise FileNotFoundError(lib_file)

if __name__ == "__main__":
  sys.exit(main())