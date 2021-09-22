import os
import sys
import subprocess
import shutil

def main():
  if len(sys.argv) < 3:
    print("Usage: python convert_dll_to_lib.py <path to the generated .lib file> <.dll file path>")
    return

  if os.environ.get('OS','') != 'Windows_NT':
    print("This script is only for Windows")
  else:
    lib_path = sys.argv[1]
    dll_file = sys.argv[2]

    assert os.path.exists(shutil.which("dumpbin"))
    assert os.path.exists(shutil.which("lib"))
    assert os.path.exists(dll_file)
    assert dll_file.find(".dll") != -1

    if not os.path.exists(lib_path):
      os.mkdir(lib_path)

    lib_name = os.path.basename(dll_file)
    lib_name = os.path.splitext(lib_name)[0]

    def_file = os.path.join(lib_path, lib_name + ".def")
    lib_file = os.path.join(lib_path, lib_name + ".lib")

    cmd_commands = [
      "echo EXPORTS >> " + def_file,
      "for /f " + '"' + "skip=19 tokens=4" + '"' + " %A in ('dumpbin /exports " + dll_file + "') do echo %A >> " + def_file,
      "lib /def:" + def_file + " /out:" + lib_file + " /machine:x64"]

    for command in cmd_commands:
      subprocess.call(command, shell=True)

if __name__ == "__main__":
  main()