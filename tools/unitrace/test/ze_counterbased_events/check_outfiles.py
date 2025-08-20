import sys
import os
import json

class Range:
  def __init__(self, start, dur):
    self.start = start
    self.end = start + dur

def calc_stats(name, rangevec):
  min_gap = 0
  max_gap = 0
  idle = 0
  total = 0
  busy = 0
  if (len(rangevec) > 0):
    v = sorted(rangevec, key=lambda r: r.start)
    first_ts = v[0].start
    last_ts = v[len(v) - 1].end
    total = last_ts - first_ts
    busy = v[0].end - v[0].start
    for i in range(1,len(v)):
      busy += v[i].end - v[i].start
      gap = v[i].start - v[i-1].end
      if (i == 1):
        min_gap = gap
        max_gap = gap
      elif (gap < min_gap):
        min_gap = gap
      elif (gap > max_gap):
        max_gap = gap
      if (gap > 0):
        idle += gap
  return {"name": name,
          "total": total,
          "busy": busy,
          "idle": idle,
          "min_gap": min_gap,
          "max_gap": max_gap,
          "overlaps": busy - (total - idle)
         }

def parse_timeline_stats(filename):
  thread_names = {}
  threads = {}

  with open(filename, 'r') as fp:
    try:
      data = json.load(fp)
    except Exception as ex:
      print("File parse errors")
      exit(-1)
    if 'traceEvents' in data:
      for e in data['traceEvents']:
        if (e["ph"] == "M"):
          if (e["name"] == "thread_name"):
            pid_k = "{}_{}".format(e["pid"], e["tid"])
            thread_names[pid_k] = e["args"]["name"]
          elif (e["name"] == "process_name"):
            pid_k = "{}_{}".format(e["pid"], e["pid"])
            thread_names[pid_k] = e["args"]["name"]
        elif (e["ph"] == "X" and e["cat"] == "gpu_op" and "pid" in e and "tid" in e):
          pid_k = "{}_{}".format(e["pid"], e["tid"])
          if (pid_k not in threads):
            threads[pid_k] = []
          r = Range(e["ts"], e["dur"])
          threads[pid_k].append(r)
    stats = []
    for k in threads:
      if (len(threads[k]) > 0):
        stats.append(calc_stats(thread_names[k], threads[k]))
    return stats

for i in range(1,len(sys.argv)):
  if sys.argv[i].endswith(".json") and os.path.exists(sys.argv[i]):
    stats = parse_timeline_stats(sys.argv[i])
    if len(stats) < 1:
      print("[ERROR] No timelines found in: {}".format(sys.argv[i]))
      exit(-1)
    for stat in stats:
      if stat["min_gap"] < 0 or stat["overlaps"] > 0:
         print("[ERROR] overlapping execution found in {}:\n {}\n".format(sys.argv[i], stat))
         exit(-1)
exit(0)
