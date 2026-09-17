#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import argparse
import http.server
import os
import socketserver
import sys
import webbrowser
import json
import gc
import re
from pathlib import Path

# analyzeperfmetrics module is either at the same folder of this script or subfolder "metrics"
# add the paths
modpath = os.path.dirname(os.path.abspath(__file__))
sys.path.append(modpath)
sys.path.append(modpath + "/metrics")
sys.path.append(modpath + "/tracemerge")

import analyzeperfmetrics as apm
from mergetrace import merge_trace_files, MergePerfettoTraces

def parse_args(argparser):
    args = argparser.parse_args()
    if args.result_dir:
        if not os.path.exists(args.result_dir):
            argparser.error(f"Result directory '{args.result_dir}' does not exist")
        if not os.path.isdir(args.result_dir):
            argparser.error(f"Result directory '{args.result_dir}' is not a directory")
        if args.trace:
            argparser.error("--result-dir cannot be used together with --trace")
        if args.metrics:
            argparser.error("--result-dir cannot be used together with --metrics")
    else:
        if not args.trace:
            argparser.error("--trace is required when --result-dir is not provided")

    if args.ranks and not args.result_dir:
        argparser.error("--ranks can only be used with --result-dir")
    return args
            
def ParseArguments():
    argparser = argparse.ArgumentParser(description = "View trace and hardware metrics in https://ui.perfetto.dev")
    argparser.add_argument('-t', '--trace', help = "trace file (.pftrace or legacy .json)")
    argparser.add_argument('-f', '--config', help = "metric view config file ")
    argparser.add_argument('-s', '--shaderdump', help = "shader dump folder for stall analysis")
    argparser.add_argument('-m', '--metrics', help = "hardware performance metrics file in CSV format")
    argparser.add_argument('-n', '--numtopstalls', type = int, default = 10, help = "number of top most expensive stalls of each type to report for stall analysis(10 default, -1 unlimited)")
    argparser.add_argument('-g', '--demangler', help = "symbol demangler if c++filt is not available")
    argparser.add_argument('--result-dir', help = "directory that holds metrics files and trace file")
    argparser.add_argument('--ranks', type=int, nargs='+', help='list of MPI ranks to include in merge (e.g., --ranks 0 1 2)')
    
    args = parse_args(argparser)
         
    return args

class TraceLoadingHttpHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin',  'https://ui.perfetto.dev')
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

    def do_GET(self):
      super().do_GET()
      self.server.done = True

    def do_POST(self):
        pass    # do nothing

def LoadTrace(tracefile):
    port = 9001    # perfetto likes 9001
    path = os.path.abspath(tracefile)
    fname = os.path.basename(path)
    cwd = os.getcwd()     # save current working directory
    try:
        os.chdir(os.path.dirname(path))
        socketserver.TCPServer.allow_reuse_address = True
        with socketserver.TCPServer(('127.0.0.1', port), TraceLoadingHttpHandler) as httpd:
            address = f'https://ui.perfetto.dev/#!/?url=http://127.0.0.1:{port}/{fname}'
            webbrowser.open_new_tab(address)
        
            httpd.done = None
            while httpd.done is None:
                httpd.handle_request()
    finally:
        os.chdir(cwd)    # restore current working directory

def get_trace_file_path(args):
    if args.result_dir:
        result_dir = Path(args.result_dir)
        chrome_log_files = []
        ext_suffix = 'json'
        if args.ranks:
            # Search for chrome_trace.json files only in rank directories we care about
            for rank in args.ranks:
                rank_pattern = f"**/rank_{rank}.*/**/chrome_trace.json"
                chrome_log_files.extend([str(json_file) for json_file in result_dir.glob(rank_pattern)])
            if len(chrome_log_files) == 0:
                # If no chrome_trace.json files found, search for chrome_trace.pftrace files
                for rank in args.ranks:
                    rank_pattern = f"**/rank_{rank}.*/**/chrome_trace.pftrace"
                    chrome_log_files.extend([str(json_file) for json_file in result_dir.glob(rank_pattern)])
                ext_suffix = 'pftrace'
        else:
            chrome_log_files.extend([str(json_file) for json_file in result_dir.rglob('chrome_trace.json')])
            if len(chrome_log_files) == 0:
                chrome_log_files.extend([str(json_file) for json_file in result_dir.rglob('chrome_trace.pftrace')])
                ext_suffix = 'pftrace'
        if len(chrome_log_files) == 0:
            print(f'Trace file is missing under {result_dir}')
            exit(1)
        elif len(chrome_log_files) > 1:
            ranks_info = f" for ranks {args.ranks}" if args.ranks else ""
            print(f'Multiple chrome_trace.json files found under {result_dir}{ranks_info}')
            # Save the combined trace inside the result directory so future
            # uniview runs can reuse it instead of merging again. The file name
            # encodes the rank filter so different rank subsets get their own
            # cached merge and never collide.
            if args.ranks:
                ranks_suffix = '_ranks_' + '_'.join(str(r) for r in sorted(args.ranks))
            else:
                ranks_suffix = ''
            combined_file = result_dir / f'combined_trace{ranks_suffix}.{ext_suffix}'
            # Reuse an existing merge only if it is at least as new as every
            # input trace; otherwise a stale cache could hide newer data.
            if combined_file.exists():
                combined_mtime = combined_file.stat().st_mtime
                inputs_mtime = max(os.path.getmtime(f) for f in chrome_log_files)
                if combined_mtime >= inputs_mtime:
                    print(f'Reusing existing combined trace file: {combined_file}')
                    return str(combined_file)
                print(f'Combined trace file is stale, re-merging: {combined_file}')
            if ext_suffix == 'pftrace':
                success = MergePerfettoTraces(chrome_log_files, combined_file)
            else:
                success = merge_trace_files(chrome_log_files, combined_file)
            if not success:
                print(f'Failed to merge trace files under {result_dir}')
                exit(1)
            else:
                print(f'Combined trace file created: {combined_file}')
                return str(combined_file)
        else:
            print(f'Using trace file: {chrome_log_files[0]}')
            return str(chrome_log_files[0])
    else:
        if args.trace is None:
            print(f'Trace file is missing')
            exit(1)
        if not os.path.exists(args.trace):
            print(f'Trace file {args.trace} is not found')
            exit(1)
        return args.trace

def get_sorted_metrics_files(metrics_files):
    def extract_number(file_path):
        """Extract number from metrics_X.csv filename"""
        match = re.search(r'metrics_(\d+)\.csv', file_path.name)
        if match:
            try:
                return int(match.group(1))
            except (ValueError, OverflowError) as e:
                print(f'Warning: Could not parse number in {file_path.name}: {e}')
                return 0
        return 0
    
    return sorted(metrics_files, key=extract_number)

def get_metrics_files_paths(args):
    if args.result_dir:
        result_dir = Path(args.result_dir)
        metrics_files = list(result_dir.rglob('metrics/metrics_*.csv'))
        print(f'Found {len(metrics_files)} : {metrics_files}')
        sorted_metrics_files = get_sorted_metrics_files(metrics_files)
        return sorted_metrics_files
    else:
        return [Path(args.metrics)] if args.metrics else []

def main():
    args = ParseArguments();

    trace_file_path = get_trace_file_path(args)
    metrics_files_paths = get_metrics_files_paths(args)
    eustall = True
    if metrics_files_paths:
        for metrics_file in metrics_files_paths:
            if not metrics_file.exists():
                print(f'Metrics file {metrics_file} is not found')
                return 1
            
        # Use the first file to check EU stall type
        first_metrics_file = metrics_files_paths[0]
        with first_metrics_file.open('r') as fp:
            for line in fp:
                if ('GlobalInstanceId' in line):
                    eustall = False
                    break
                if ('IP[Address]' in line):
                    break

        if (eustall is False):
            if (args.config is None):
                print(f'Config file is missing')
                return 1
            else:
                if not os.path.exists(args.config):
                    print(f'Config file {args.config} is not found')
                    return 1

        if (args.shaderdump is not None):
            if not os.path.exists(args.shaderdump):
                print(f'Shader dump folder {args.shaderdump} is not found')
                return 1

    else:
        if ((args.config is not None) or (args.shaderdump is not None)):
            print(f'Metrics file is missing')
            return 1

    # validate trace file
    is_perfetto = trace_file_path.endswith(".pftrace")
    fsize = os.stat(trace_file_path).st_size
    if (fsize != 0):
        # Perfetto protobuf is binary and self-terminating; skip the JSON
        # validity check and closing-tag repair (those are JSON-only).
        # ui.perfetto.dev auto-detects the protobuf format.
        if (is_perfetto == False):
            valid = True
            with open(trace_file_path, 'r') as fp:
                try:
                    data = json.load(fp)
                    del data
                    gc.collect()
                except Exception as ex:
                    valid = False
            if (valid == False):
                # trace file may not be closely closed
                # Try to add closing tags
                with open(trace_file_path, 'a') as fp:
                    try:
                        fp.write("\n]\n}\n")
                    except Exception as ex:
                        # give up
                        print("Failed to add closing tags to trace file " + trace_file_path)
                        return 1
                # read the file again
                rollback = False
                with open(trace_file_path, 'r') as fp:
                    try:
                        data = json.load(fp)
                        del data
                        gc.collect()
                    except Exception as ex:
                        # give up, but need to roll back the changes made to the file
                        rollback = True
                if (rollback == True):
                    with open(trace_file_path, 'a') as fp:
                        try:
                            fp.truncate(fsize)
                        except Exception as ex:
                            print("Failed to rollback the changes to trace file " + trace_file_path)
                    print("Trace file " + trace_file_path + " is invalid")
                    return 1
                else:
                    print("Trace file " + trace_file_path + " is modified with proper closing tags added")
    else:
        print("Trace file " + trace_file_path + " is empty")
        return 1

    LoadTrace(trace_file_path)

    if metrics_files_paths:
        https = False
        # The https-vs-local decision is driven by a metrics URL embedded in the
        # trace. For JSON it can be found by a text scan; the binary
        # Perfetto trace carries it in a debug annotation, so skip the scan and
        # use the local (-q) path, matching the http://localhost URL the emitter
        # writes for protobuf output.
        if (is_perfetto == False):
            with open(trace_file_path, 'r') as fp:
                for num, line in enumerate(fp):
                    if ("https://" in line):
                        https = True
                        break
                    elif ("http://" in line):
                        break

        options = []
        if (eustall is True):
            if (args.shaderdump is not None):
                options.extend(['-s', args.shaderdump])
            if (args.demangler is not None):
                options.extend(['-g', args.demangler])
            options.extend(['-n', str(args.numtopstalls)])
        else:
            if (args.config is not None):
                options = ['-f', str(args.config)]

        if (https == True):
            options.append('-p')
        else:
            options.append('-q')
        if args.result_dir:
            options.extend(['--result-dir', str(args.result_dir)])
        else:
            options.append(str(first_metrics_file))

        apm.main(apm.ParseArguments(options))

if __name__ == '__main__':
    sys.exit(main())
