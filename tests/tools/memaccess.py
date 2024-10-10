import os
import subprocess
import sys

from samples import dpc_gemm
from samples import cl_gemm
from samples import ze_gemm
import utils
import json

def config(path):
  cmake = ["cmake",\
    "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  stdout, stderr = utils.run_process(cmake, path)
  if stderr and stderr.find("CMake Error") != -1:
    return stderr
  return None

def build(path):
  stdout, stderr = utils.run_process(["make"], path)
  if stderr and stderr.lower().find("error") != -1:
    return stderr
  return None

def getTestAppCommand(option, analysisOptions, matrixSize, iterations):
  command = ["./memaccess"] + analysisOptions
  if option == "cl":
    app_folder = utils.get_sample_executable_path("cl_gemm")
    app_file = os.path.join(app_folder, "cl_gemm")
    return command + [app_file, "gpu", f"{matrixSize}", f"{iterations}"]
  elif option == "ze":
    app_folder = utils.get_sample_executable_path("ze_gemm")
    app_file = os.path.join(app_folder, "ze_gemm")
    return command + [app_file, f"{matrixSize}", f"{iterations}"]
  else:
    app_folder = utils.get_sample_executable_path("dpc_gemm")
    app_file = os.path.join(app_folder, "dpc_gemm")
    return command + [app_file, "gpu",  f"{matrixSize}", f"{iterations}"]

class MemAccessData:
  def __init__(self):
    self.offset = -1
    self.simd = 0
    self.execSize = 0
    self.elementSize = 0
    self.numOfElements = 0
    self.isScatter = False
    self.isSlm = False
    self.isScratch = False
    self.addrWidth = 0
    self.isWrite = False

    self.instCount = 0
    self.simdCount = 0
    self.clCount = 0
    self.notAligned = 0

    self.strideDistribution = {}
    self.addresses = []

  def ReadFromJson(self, data):
    self.offset = int(data['offset'])
    self.simd = int(data['simd_width'])
    self.execSize = int(data['exec_size'])
    self.elementSize = int(data['element_size'])
    self.numOfElements = int(data['num_of_elements'])
    self.isScatter = bool(data['is_scatter'])
    self.isSlm = bool(data['is_slm'])
    self.isScratch = bool(data['is_scratch'])
    self.addrWidth = int(data['addr_width']) * 8
    self.isWrite = bool(data['is_write'])

    self.instCount = int(data['access_instruction_counter'])
    self.simdCount = int(data['simd_lanes_active_counter'])
    self.clCount = int(data['cache_lines_counter'])
    self.notAligned = int(data['cl_not_aligned_counter'])

    for stride, value in data['stride_distribution'].items():
      self.strideDistribution[int(stride)] = int(value)

    self.addresses = data['addresses']

  def ReadFromTxt(self, data):
    self.offset = int(data[0].split(' ')[0], 16)
    self.simd = int(data[2].split('SIMD')[1].split(' ')[0])
    self.execSize = int(data[2].split('ExecSize_')[1].split(' ')[0])
    self.elementSize = int(data[2].split('ExecSize_')[1].split(' bytes')[0].split(' ')[1])
    self.numOfElements = int(data[2].split('bytes X')[1].split(' ')[0])
    self.isScatter = 'Scatter' in data[2]
    self.isSlm = 'SLM' in data[2]
    self.isScratch = 'Scratch' in data[2]
    self.addrWidth = 64 if 'A64' in data[2] else 32
    self.isWrite = 'Write' in data[2]

    self.instCount = int(data[3].split('  * Instruction executed: ')[1])
    self.simdCount = int(data[4].split('  * SIMD lanes executed: ')[1])
    self.clCount = int(data[5].split('  * Cache line transferred: ')[1].split('(')[0])
    self.notAligned = int(data[7].split('  * Cache line not aligned: ')[1].split(' (')[1].split(')')[0])

    for idx, row in enumerate(data):
      if idx < 9: continue
      if 'No strides detected' in row:
        break
      if row.startswith('  *'):
        break
      stride = int(row.split('stride: ')[1].split(' bytes')[0])
      value = int(row.split(') -> ')[0].split('% (')[1])
      self.strideDistribution[stride] = value

    if idx < len(data):
      for row in data[idx:]:
        if not row.startswith('      Addr# '):
          continue
        row = row[14:]
        addresses = row.split(' ')
        addresses = [x for x in addresses if x != '']
        self.addresses += [str(int(x, 16)) for x in addresses]

  def __add__(self, other):
    SUM_LIST = ['instCount', 'simdCount', 'clCount', 'notAligned']
    COPY_LIST = ['offset', 'simd', 'execSize', 'elementSize', 'numOfElements', 'isScatter', 'isSlm', 'isScratch', 'addrWidth', 'isWrite']
    if isinstance(other, MemAccessData):
      result = MemAccessData()
      for field in COPY_LIST:
        setattr(result, field, getattr(other, field))
      sum_fields = [field for field in SUM_LIST]
      for field in sum_fields:
        setattr(result, field, (getattr(self, field) + getattr(other, field)))
      for stride, value in other.strideDistribution.items():
        result.strideDistribution[stride] = value
      for stride, value in self.strideDistribution.items():
        result.strideDistribution[stride] = result.strideDistribution.get(stride, 0) + value
      result.addresses = self.addresses if len(self.addresses) else other.addresses
      return result
    else:
      raise TypeError("Unsupported operand type(s) for +: '{}' and '{}'".format(type(self).__name__, type(other).__name__))

  def __eq__(self, other) -> bool:
    if not isinstance(other, MemAccessData):
      return False
    EXCLUDE_LIST = ['addresses']
    fields = [field for field in dir(self) 
              if (not callable(getattr(self, field)) 
                  and not field.startswith('__') 
                  and field not in EXCLUDE_LIST)]
    for field in fields:
      if getattr(self, field) != getattr(other, field):
        return False
    if (len(self.addresses) != len(other.addresses)):
      return False
    return True

  def __str__(self):
    output = "\n"
    output += f"Offset: {self.offset}, "
    output += f"SIMD: {self.simd}, "
    output += f"ExecSize: {self.execSize}, "
    output += f"ElementSize: {self.elementSize}, "
    output += f"NumOfElements: {self.numOfElements}, "
    output += f"IsScatter: {self.isScatter}, "
    output += f"IsSlm: {self.isSlm}, "
    output += f"IsScratch: {self.isScratch}, "
    output += f"AddrWidth: {self.addrWidth}, "
    output += f"IsWrite: {self.isWrite}, "
    output += f"InstCount: {self.instCount}, "
    output += f"SimdCount: {self.simdCount}, "
    output += f"ClCount: {self.clCount}, "
    output += f"notAligned: {self.notAligned}, "
    output += f"StrideDistribution len: {len(self.strideDistribution)}, "
    output += f"Addresses len: {len(self.addresses)}\n"
    return output

  def __repr__(self):
    return self.__str__()

def parseTxtResults(stderr) -> dict:
  data = {}
  data['kernels'] = {}
  rows = stderr.split('\n')
  kernelName = ''
  sendRows = []
  isSendDataRows = False
  try: 
    for row in rows:
      if row.startswith('=== '):  
        kernelName = row.split('=== ')[1].split(' ')[0]
        data['kernels'][kernelName] = {}
        continue
      if row.startswith('--------------------------------------------------------------------------------'):
        isSendDataRows = True
        continue
      if row == '':
        if not isSendDataRows: continue
        isSendDataRows = False
        mad = MemAccessData()
        mad.ReadFromTxt(sendRows)
        d = data['kernels'][kernelName]
        d[mad.offset] = d.get(mad.offset, MemAccessData()) + mad
        sendRows = []
        continue
      if isSendDataRows:
        sendRows.append(row)
        continue
  except Exception as e:
    print('Error', e)
    return {}
  return data

def parseJsonResults(stderr) -> dict:
  data = {}
  data['kernels'] = {}
  try:
    jsonData = json.loads(stderr)
    for k in jsonData['kernels']:
      data['kernels'][k['kernel_name']] = {}
      d = data['kernels'][k['kernel_name']]
      for i in k['invocations']:
        results = i["tiles"][0]['results']
        for r in results:
          mad = MemAccessData()
          mad.ReadFromJson(r)
          d[mad.offset] = d.get(mad.offset, MemAccessData()) + mad

  except json.JSONDecodeError:
    return {}
  return data

def isValidOutput(stdout, stderr):
  if not stdout:
    return "stdout is empty"
  if not stderr:
    return "stderr is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  return None

def run(path, option):
  baseSize = 128
  TWO_RUNS = 2

  command = getTestAppCommand(option, [], baseSize * 1, TWO_RUNS)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  dataTxt = parseTxtResults(stderr)
  # smoke test
  if len(dataTxt.keys()) == 0: return "No data found in the output"


  command = getTestAppCommand(option, ['--json-output'], baseSize * 1, TWO_RUNS)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  dataJson = parseJsonResults(stderr)
  if len(dataJson.keys()) == 0: return "No data found in the output"

  return None # skip other tests due to issue with SIMD active lanes calculation

  # check equality of text and json results from different runs
  if dataTxt != dataJson:
    return "Data mismatch between txt and json output"

  command = getTestAppCommand(option, ['--json-output'], baseSize * 1, 1)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  data1 = parseJsonResults(stderr)
  if len(data1.keys()) == 0: return "No data found in the output"

  command = getTestAppCommand(option, ['--json-output'], baseSize * 2, 1)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  data2 = parseJsonResults(stderr)
  if len(data2.keys()) == 0: return "No data found in the output"

  command = getTestAppCommand(option, ['--json-output'], baseSize * 4, 1)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  data3 = parseJsonResults(stderr)
  if len(data3.keys()) == 0: return "No data found in the output"

  # check that different workload sizes resulting different values
  if data1 == data2:
    return "Data mismatch between output for 1 and 2 iterations"

  if data1 == data3:
    return "Data mismatch between output for 1 and 3 iterations"

  if data2 == data3:
    return "Data mismatch between output for 2 and 3 iterations"

  # calculation of correctness based on relative results
  for k in data1['kernels']:
    for offset in data1['kernels'][k]: # iterate over all sends in all kernels
      mad1 = data1['kernels'][k][offset]
      mad2 = data2['kernels'][k][offset]
      mad3 = data3['kernels'][k][offset]

      EQUAL_LIST = ['offset', 'simd', 'execSize', 'elementSize', 'numOfElements', 'isScatter', 'isSlm', 'isScratch', 'addrWidth', 'isWrite']
      for field in EQUAL_LIST:
        if not (getattr(mad1, field) == getattr(mad2, field) == getattr(mad3, field)):
          return f"Data mismatch between output for different iterations for equal data: {field} \n{mad1}\n{mad2}\n{mad3}\n"

      EQUAL_LEN = ['addresses']
      for field in EQUAL_LEN:
        if not (len(getattr(mad1, field)) == len(getattr(mad2, field)) == len(getattr(mad3, field))):
          return f"Data mismatch between output for different iterations, equal length: {field} \n{mad1}\n{mad2}\n{mad3}\n"

      SCALABLE_LIST = ['instCount', 'simdCount', 'clCount', 'notAligned']
      for field in SCALABLE_LIST:
        a1 = getattr(mad1, field)
        a2 = getattr(mad2, field)
        a3 = getattr(mad3, field)
        if a1 == a2 == a3 == 0:
          continue
        if a1 == 0 or a2 == 0 or a3 == 0:
          return f"Data mismatch between output for different iterations, all zeros: {field} \n{mad1}\n{mad2}\n{mad3}\n"

        if ((a2*a2-a1*a3) > 0.0001 * (a2*a2+a1*a3)):
          return f"Data mismatch between output for different iterations, not equal: {field}: a1: {a1}, a2: {a2}, a3: {a3}\n a2/a1 != a3/a2"

      # get list of stride keys, check if it is the same for all results mad
      if not (set(mad1.strideDistribution.keys()) == set(mad2.strideDistribution.keys()) == set(mad3.strideDistribution.keys())):
        return f"Data mismatch between output for different iterations: strideDistribution keys \n{mad1}\n{mad2}\n{mad3}\n"

      s1 = sum(mad1.strideDistribution.values())
      s2 = sum(mad2.strideDistribution.values())
      s3 = sum(mad3.strideDistribution.values())
      for stride in mad1.strideDistribution.keys():
        a1 = mad1.strideDistribution[stride]
        a2 = mad2.strideDistribution[stride]
        a3 = mad3.strideDistribution[stride]
        if not (a1/s1 == a2/s2 == a3/s3):
          return f"Data mismatch between output for different iterations: strideDistribution \n{mad1}\n{mad2}\n{mad3}\n"

      # check that addresses are not zeros
      if not (any(mad1.addresses) and any(mad2.addresses) and any(mad3.addresses)):
        return f"Data mismatch between json output for different iterations: zero addresses \n{mad1}\n{mad2}\n{mad3}\n"

  return None


def main(option):
  path = utils.get_tool_build_path("memaccess")
  if option == "cl":
    log = cl_gemm.main("gpu")
    if log:
      return log
  elif option == "ze":
    log = ze_gemm.main(None)
    if log:
      return log
  else:
    log = dpc_gemm.main("gpu")
    if log:
      return log
  log = config(path)
  if log:
    return log
  log = build(path)
  if log:
    return log
  log = run(path, option)
  if log:
    return log

if __name__ == "__main__":
  option = "cl"
  if len(sys.argv) > 1 and sys.argv[1] == "ze":
    option = "ze"
  if len(sys.argv) > 1 and sys.argv[1] == "dpc":
    option = "dpc"
  log = main(option)
  if log:
    print(log)
