import argparse
import datetime
import roofline_libs as tools  # Ensure this module is available in your environment
import os
import glob
import shutil
import textwrap 


def parse_arguments():
    """Parse command-line arguments."""
    description = textwrap.dedent('''\
        GPU Kernel Roofline

        Usage:
          run unitrace and roofline model at the same time:
             --app <application> --device <device-config> --output <output-file> 
          
          or
          
          run unitrace to profile the application, then run roofline model:
             --compute <compute-profile> --memory <memory-profile> --device <device-config> --output <output-file>

        Note: 
             --device is always required.
              
             You must provide either --app (application to profile) or both --compute and --memory:  
                                 
                To create <compute-profile> file using unitrace :
                    unitrace --g ComputeBasic -q --chrome-kernel-logging -o <compute-profile> <app>
                To create <memory-profile> file using unitrace : 
                    unitrace --g VectorEngine138 -q --chrome-kernel-logging -o <memory-profile> <app>                            

            Use --unitrace to specify the path to unitrace executable if it is not set in PATH.

        Examples:
          1. To profile an application:

             python /path/to//roofline.py  --device /path/to/device_configs/PVC_1tile.csv --output demm  --app ./a.out

          2. To use pre-generated metrics files:

             python /path/to/roofline.py --device device_configs/PVC_1tile.csv --compute example/gemm_fp16_flops.1799853  --memory  example/gemm_fp16_bytes.1799860  --output toto
            
          3. To specify the path to the unitrace executable:
             python /path/to/roofline.py  --app ./my_application --device ./device_config.json --output result.html --unitrace /path/to/unitrace --output toto
    ''')

    parser = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.RawTextHelpFormatter
    )

    parser.add_argument('--compute', required=False,
                        help='compute profile created by unitrace')
    parser.add_argument('--memory', required=False,
                        help='memory profile created by unitrace')
    parser.add_argument('--app', required=False, help='application to profile')
    parser.add_argument('--device', required=True, help='device configuration file')
    parser.add_argument('--output', required=True, help='output file in HTML format')
    parser.add_argument('--unitrace', required=False, help='path to unitrace executable if it is not set in PATH')
    args = parser.parse_args()

    # Ensure that either --app is provided or both --compute and --memory are provided
    if not args.app and not (args.compute and args.memory):
        parser.error("You must provide either --app or both --compute and --memory")

    return args


def find_unitrace(unitrace_path=None):
    """Find the unitrace executable."""
    if unitrace_path:
        if os.path.isfile(unitrace_path) and os.access(unitrace_path, os.X_OK):
            return unitrace_path
        else:
            raise FileNotFoundError(f"The specified unitrace path '{unitrace_path}' is not valid or not executable.")
    else:
        unitrace_path = shutil.which('unitrace')
        if unitrace_path:
            return unitrace_path
        else:
            raise EnvironmentError("The unitrace executable is not found in the PATH and no path was provided.")


def create_output_directory():
    """Create a directory with the current date."""
    current_datetime = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    output_dir = os.path.join(os.getcwd(), f"Unitrace_result_{current_datetime}")
    os.makedirs(output_dir, exist_ok=True)
    return output_dir


def run_unitrace_commands(app, output_dir, unitrace_path):
    """Run unitrace commands to generate metrics files."""
    commands = [
        [
            unitrace_path,
            '--group', 'ComputeBasic',
            '--demangle', '-q',
            '--chrome-kernel-logging',
            '-o', os.path.join(output_dir, '_IntelGPUBasic'),
            app
        ],
        [
            unitrace_path,
            '--group', 'VectorEngine138',
            '--demangle', '-q',
            '--chrome-kernel-logging',
            '-o', os.path.join(output_dir, '_IntelGPUCompute'),
            app
        ]
    ]

    # Execute the commands
    for command in commands:
        print("The command is", command)
        os.system(' '.join(command))


def merge_metrics_files(output_dir, pattern, output_file):
    """Merge all metrics files matching the pattern into a single file."""
    merged_path = os.path.join(output_dir, output_file)
    with open(merged_path, 'w') as merged_file:
        for file_path in glob.glob(os.path.join(output_dir, pattern)):
            with open(file_path, 'r') as f:
                merged_file.write(f.read())
                merged_file.write('\n')  # Add a newline between files for clarity
    print(f"All {pattern} files have been merged into {merged_path}")
    return merged_path


def process_results(compute, memory, device, output_name):
    """Process the results and generate the roofline plot."""
    current_platform = tools.Platform(device)
    result = tools.read_unitrace_report(compute, memory)
    tools.summary(result, current_platform)
    tools.plot_html_roofline(result, current_platform, output_name)


def main():
    args = parse_arguments()

    if args.app:
        unitrace_path = find_unitrace(args.unitrace)
        output_dir = create_output_directory()
        run_unitrace_commands(args.app, output_dir, unitrace_path)
        merged_flops_path = merge_metrics_files(output_dir, '*_IntelGPUCompute.metrics*', 'flops.txt')
        merged_bytes_path = merge_metrics_files(output_dir, '*_IntelGPUBasic.metrics*', 'bytes.txt')
    else:
        merged_flops_path = args.compute
        merged_bytes_path = args.memory

    process_results(merged_flops_path, merged_bytes_path, args.device, args.output)

    # Remove the output directory
    if args.app:
        shutil.rmtree(output_dir, ignore_errors=True)
        print(f"Output directory {output_dir} has been removed.")


if __name__ == "__main__":
    main()
