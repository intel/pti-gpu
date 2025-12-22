import os
import re
import sys
import argparse


def get_kernels_names(log_file_path):
    kernels = []
    with open(log_file_path) as f:
        lines = f.readlines()

    # Find the line with '=== Kernel Properties ==='
    for i, line in enumerate(lines):
        if line.strip() == "=== Kernel Properties ===":
            start_idx = i + 3  # Skip the header line after the marker
            break
    else:
        start_idx = None

    if start_idx is not None:
        for line in lines[start_idx:]:
            line = line.strip()
            
            # skip empty or section lines
            if not line or line.startswith('='):
                continue  
            match = re.match(r'"([^"]+)"', line)
            if match:
                kernel = match.group(1)
                kernels.append(kernel)
    return kernels

def verify_kernels_num(kernel_names, include_kernels_values, exclude_kernels_values):
    first_include_pattern = include_kernels_values[0] if include_kernels_values else None
    first_exclude_pattern = exclude_kernels_values[0] if exclude_kernels_values else None
    second_exclude_pattern = exclude_kernels_values[1] if len(exclude_kernels_values) > 1 else None
    expected_kernels_num = -1
    
    # verify number of included kernels
    if first_include_pattern == "not_exists":
        expected_kernels_num = 0
    elif first_include_pattern == "sycl" and not exclude_kernels_values:
        expected_kernels_num = 3
    elif first_exclude_pattern == "sycl":
        expected_kernels_num = 0
    elif (first_include_pattern == "sycl" and
          first_exclude_pattern == "2}" and
          second_exclude_pattern == "EE1_"):
        expected_kernels_num = 2

    if len(kernel_names) != expected_kernels_num:
        print(f"[ERROR] should be {expected_kernels_num} kernels, but found {len(kernel_names)} kernels")
        sys.exit(-1)
        
def main():
    parser = argparse.ArgumentParser(description="Check timeline stats in JSON trace files.")
    parser.add_argument('output_files', nargs='+', help='output files')
    parser.add_argument('--cmd', type=str, nargs=argparse.REMAINDER, help='command argument') 
    args = parser.parse_args()

    include_kernels_values = []
    exclude_kernels_values = []
   
   
    if args.output_files is None or len(args.output_files) != 1:
        print(f"[ERROR] should be one output file")
        sys.exit(-1)

    kernel_names = get_kernels_names(args.output_files[0])
    

    if args.cmd:
        if '--include-kernels' in args.cmd:
            idx = args.cmd.index('--include-kernels') + 1
            include_kernels_values = args.cmd[idx].split(',')

        if '--exclude-kernels' in args.cmd:
            idx = args.cmd.index('--exclude-kernels') + 1
            exclude_kernels_values = args.cmd[idx].split(',')

    verify_kernels_num(kernel_names, include_kernels_values, exclude_kernels_values)
    
    for kernel_name in kernel_names:
                    
        # verify that no excluded kernels were handled
        for exclude_kernel in exclude_kernels_values:
            if exclude_kernel in kernel_name:
                print(f"[ERROR] kernel {kernel_name}, exclude_kernel value : {exclude_kernel}")
                sys.exit(-1)
        if include_kernels_values:
            
            # verify that all handled kernels should have been included
            kernel_included = False
            for include_kernel in include_kernels_values:
                if include_kernel in kernel_name:
                    kernel_included = True
                    break
            if not kernel_included:
                print(f"[ERROR] kernel {kernel_name} not in include_kernels values: {include_kernels_values}")
                sys.exit(-1)

if __name__ == "__main__":
  main()