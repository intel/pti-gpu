# Categorizing GPU kernels

## Overview

The summarizing and categorizing scripts analyze unitrace reports and aggregate kernels by categories. The summary is stored in `JSON` format (see example below) for further analysis or as input to other tools.

```json
{
  "allreduce_time": 660801949838.875,
  "allreduce_calls": 6937803.0,
  "matmul_time": 163155211000.0,
  "matmul_calls": 11520000.0,
  "attn_time": 53528349820.0,
  "attn_calls": 3276800.0,
  "norm_time": 26989135820.0,
  "norm_calls": 3379201.0,
  "mem_op_time": 4660369802.857142,
  "mem_op_calls": 873879.0,
}
```

## Run

Before runing the script, all summary outputs from all processes should be packed into a single `tarball`. 
The summary script will aggregate kernels from all processes before categorizing them.

```sh
unitrace -h -d -r mpirun -np 4 python ...
tar cfz unitrace_4_mpi_100iter.tgz output.*
summary.py --input unitrace_4_mpi_100iter.tgz --schema schemas/LLaMA.json --output summary_4_mpi_100iter.json
```

## Create Schemas

A class of workloads can have multiple kernels. A scheme can be created in the INI file format:

```ini
[matmul if equals to]
gemm_kernel

[matmul if starts with]
xpu::xetla::hgemm_caller
xpu::xetla::HgemmQKVKernel

[allreduce if ends with]
ALLREDUCE_SMALL
ALLREDUCE_MEDIUM
ALLREDUCE_LARGE
```

Each section starts with a category name (matmul, allreduce etc.) follwed by a condition.
There are 3 kinds of conditions: equals to, starts with and ends with. Content of sections are 
kernel names.

Once a schema is created, it can be converted to JSON format to be used with `summary.py`

```sh
categorize.py --input LLaMA.ini --output LLaMA.json
```
