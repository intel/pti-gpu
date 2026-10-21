# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Run the --metric-config parser unit test (config_node_test). No GPU needed.

import os
import platform
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        print(f"[ERROR] usage: {sys.argv[0]} <test-bin-dir>", file=sys.stderr)
        return 1
    name = "config_node_test.exe" if platform.system() == "Windows" else "config_node_test"
    binary = os.path.join(sys.argv[1], "metric_config", name)
    if not os.path.exists(binary):
        print(f"[ERROR] {binary} not found", file=sys.stderr)
        return 1
    return subprocess.run([binary]).returncode


if __name__ == "__main__":
    sys.exit(main())
