#!/usr/bin/env python3
#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import argparse
import json
import shutil
from pathlib import Path

def ParseCommandLineArgs():
    parser = argparse.ArgumentParser(description = 'Merge unitrace result files')
    parser.add_argument('inputFiles', nargs = '*', help = 'list of files to merge')
    parser.add_argument('--result-dir', help = 'path to result directory containing trace files')    
    parser.add_argument('--ranks', type=int, nargs = '+', help = 'list of MPI ranks to include in merge (e.g., --ranks 0 1 2)')
    parser.add_argument('-o', '--outputFile', default = 'unitrace.all.json', help = 'output file')

    args = parser.parse_args()

    if args.result_dir and args.inputFiles:
        parser.error("Cannot use both input files and --result-dir")
    
    if not args.result_dir and not args.inputFiles:
        parser.error("Must provide either input files or --result-dir")

    if args.ranks and not args.result_dir:
        parser.error("--ranks can only be used with --result-dir")
    
    input_files = []
    if args.result_dir:
        result_dir = Path(args.result_dir)
        if not result_dir.exists():
            parser.error(f"Directory '{args.result_dir}' does not exist")

        if not result_dir.is_dir():
            parser.error(f"'{args.result_dir}' is not a directory")

        if args.ranks:
            input_files = []
            for rank in args.ranks:
                rank_pattern = f"**/rank_{rank}.*/**/chrome_trace.json"
                input_files.extend([str(json_file) for json_file in result_dir.glob(rank_pattern)])
            if (len(input_files) == 0):
                for rank in args.ranks:
                    rank_pattern = f"**/rank_{rank}.*/**/chrome_trace.pftrace"
                    input_files.extend([str(json_file) for json_file in result_dir.glob(rank_pattern)])
            if (len(input_files) == 0):
                print(f'Trace file is missing under {result_dir}')
                exit(1)
        else:
            input_files = [str(json_file) for json_file in result_dir.rglob('chrome_trace.json')]
            if not input_files:
                input_files = [str(json_file) for json_file in result_dir.rglob('chrome_trace.pftrace')]
                if not input_files:
                    print(f'Trace file is missing under {result_dir}')
                    exit(1)
    else:
        input_files = args.inputFiles
        if not input_files:
            print("No input files provided")
            exit(1)

    return input_files, args.outputFile

def MergePerfettoTraces(inputFiles, outputFile):
    # Perfetto protobuf traces are a sequence of TracePacket messages; the
    # concatenation of valid traces is itself a valid trace, and unitrace emits
    # all ranks on a shared clock domain (REALTIME/BOOTTIME), so they align.
    # Merging is therefore a plain byte concatenation -- no JSON repair needed.
    try:
        with open(outputFile, 'wb') as ofp:
            for f in inputFiles:
                if os.stat(f).st_size == 0:
                    continue
                with open(f, 'rb') as fp:
                    shutil.copyfileobj(fp, ofp)  # streamed copy, bounded memory
        return True

    except Exception as e:
        print(f"Error merging trace files: {e}")
        return False

def merge_trace_files(input_files, output_file):
    """
    Merge multiple unitrace JSON files into a single output file.
    
    Args:
        input_files (list): List of input file paths to merge
        output_file (str): Path to the output file
    
    Returns:
        bool: True if successful, False otherwise
    """
    try:
        with open(output_file, 'w') as ofp:
            ofp.write('{\n')
            ofp.write('"traceEvents": [\n')
            has_events = False

            for i in range(0, len(input_files)):
                fsize = os.stat(input_files[i]).st_size
                if (fsize != 0):
                    valid = True
                    with open(input_files[i], 'r') as fp:
                        try:
                            data = json.load(fp)
                        except Exception as ex:
                            valid = False
                    if (valid == False):
                        # closing tags are likely missing in most cases
                        # add closing tags
                        with open(input_files[i], 'a') as fp:
                            try:
                                fp.write("\n]\n}\n")
                            except Exception as ex:
                                # give up
                                print("Failed to add closing tags to trace file " + input_files[i])
                                continue

                        # read the file again
                        rollback = False
                        with open(input_files[i], 'r') as fp:
                            try:
                                data = json.load(fp)
                            except Exception as ex:
                                # give up, but need to roll back the changes made to the file
                                rollback = True
                        if (rollback == True):
                            with open(input_files[i], 'a') as fp:
                                try:
                                    fp.truncate(fsize)
                                except Exception as ex:
                                    print("Failed to rollback the changes to trace file " + input_files[i])
                            print("Skip invalid trace file " + input_files[i])
                            continue
                        else:
                            print("File " + input_files[i] + " is modified with proper closing tags added")

                    if 'traceEvents' in data:
                        for e in data['traceEvents']:
                            ofp.write(json.dumps(e))
                            ofp.write(',\n')
                            has_events = True

            if has_events:
                pos = ofp.tell() - 2  # undo last ',\n'
                ofp.seek(pos, os.SEEK_SET)
            ofp.write('\n]\n')
            ofp.write('}\n')
            
        return True
        
    except Exception as e:
        print(f"Error merging trace files: {e}")
        return False

if __name__ == "__main__":

    inputFiles, outputFile = ParseCommandLineArgs()

    # Route by format: .pftrace inputs merge by byte concatenation;
    # legacy .json inputs by the JSON path below. The two formats cannot be
    # merged together, so reject a mixed set
    perfetto_inputs = [f for f in inputFiles if f.endswith(".pftrace")]
    if perfetto_inputs or outputFile.endswith(".pftrace"):
        if len(perfetto_inputs) != len(inputFiles):
            json_inputs = [f for f in inputFiles if f not in perfetto_inputs]
            print("[ERROR] cannot merge mixed trace formats; got .pftrace "
                  "inputs {} and non-perfetto inputs {}. Merge each format "
                  "separately.".format(perfetto_inputs, json_inputs))
            sys.exit(1)
        MergePerfettoTraces(perfetto_inputs, outputFile)
        sys.exit(0)

    success = merge_trace_files(inputFiles, outputFile)
    if success:
        print(f"Successfully merged {len(inputFiles)} files into {outputFile}")
        sys.exit(0)
    else:
        print("Failed to merge trace files")
        sys.exit(1)
