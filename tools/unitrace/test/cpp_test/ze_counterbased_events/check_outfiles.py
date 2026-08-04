
import os
import sys
import argparse

# Shared trace parsers live in tools/unitrace/test/trace_utils/trace_compare.py
# (this file is tools/unitrace/test/cpp_test/ze_counterbased_events/...).
sys.path.insert(0, os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "trace_utils")))
from trace_compare import parse_timeline_stats, parse_timeline_stats_perfetto


def main():
  parser = argparse.ArgumentParser(description="Check timeline stats in trace files (JSON or Perfetto protobuf).")
  parser.add_argument('output_files', nargs='+', help='output files')
  parser.add_argument('--cmd', type=str, nargs=argparse.REMAINDER, help='command argument')
  args = parser.parse_args()

  for filename in args.output_files:
    if filename.endswith(".json") and os.path.exists(filename):
      stats = parse_timeline_stats(filename)
    elif filename.endswith(".pftrace") and os.path.exists(filename):
      stats = parse_timeline_stats_perfetto(filename)
    else:
      continue
    if len(stats) < 1:
      print(f"[ERROR] No timelines found in: {filename}")
      exit(-1)
    for stat in stats:
      if stat["min_gap"] < 0 or stat["overlaps"] > 0:
        print(f"[ERROR] overlapping execution found in {filename}:\n {stat}\n")
        exit(-1)
  exit(0)

if __name__ == "__main__":
  main()
