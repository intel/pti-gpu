import os
import sys
import shutil
import subprocess

def clone(url, commit, clone_path):
  if (not os.path.exists(clone_path)):
    subprocess.call(["git", "clone", url, clone_path])
    subprocess.call(["git", "checkout", commit], cwd = os.path.abspath(clone_path))

def copy(src_path, dst_path, file_list):
  for file in file_list:
    dst_file = os.path.join(dst_path, file)
    if os.path.isfile(dst_file):
      os.remove(dst_file)

    src_file = os.path.join(src_path, file)
    assert os.path.isfile(src_file)

    shutil.copy(src_file, dst_file)

def download(url, download_path):
  if not os.path.exists(download_path):
    os.mkdir(download_path)

  url_items = url.split('/')
  file_name = os.path.join(download_path, url_items[len(url_items) - 1])
  command = "curl " + url + " --output " + file_name
  shell = True
  if sys.platform != 'win32':
    shell = False
    command = command.split(" ")

  if not os.path.isfile(file_name):
    subprocess.call(command, shell = shell)

  return file_name

def unpack(arch_file, target_path):
  if (not os.path.exists(target_path)):
    os.mkdir(target_path)
  subprocess.call(["tar", "-xf", arch_file, "-C", target_path])

def get_root(build_path):
  sample_path, build_dir = os.path.split(build_path)
  samples_path, sample_dir = os.path.split(sample_path)
  root_path, samples_dir = os.path.split(samples_path)
  return root_path