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

def ParseCommandLineArgs():
    parser = argparse.ArgumentParser(description = 'Merge unitrace result files')
    parser.add_argument('inputFiles', nargs = '+', help = 'list of files to merge')
    parser.add_argument('-o', '--outputFile', default = 'unitrace.all.json', help = 'output file')

    args = parser.parse_args()

    return (args.inputFiles, args.outputFile)

def MergePerfettoTraces(inputFiles, outputFile):
    # Perfetto protobuf traces are a sequence of TracePacket messages; the
    # concatenation of valid traces is itself a valid trace, and unitrace emits
    # all ranks on a shared clock domain (REALTIME/BOOTTIME), so they align.
    # Merging is therefore a plain byte concatenation -- no JSON repair needed.
    with open(outputFile, 'wb') as ofp:
        for f in inputFiles:
            try:
                if os.stat(f).st_size == 0:
                    continue
                with open(f, 'rb') as fp:
                    shutil.copyfileobj(fp, ofp)  # streamed copy, bounded memory
            except Exception as ex:
                print("Skip trace file " + f + ": " + str(ex))

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

    with open(outputFile, 'w') as ofp:
        ofp.write('{\n')
        ofp.write('"traceEvents": [\n')

        for i in range(0, len(inputFiles)):
            fsize = os.stat(inputFiles[i]).st_size
            if (fsize != 0):
                valid = True
                with open(inputFiles[i], 'r') as fp:
                    try:
                        data = json.load(fp)
                    except Exception as ex:
                        valid = False
                if (valid == False):
                    # closing tags are likely missing in most cases
                    # add closing tags
                    with open(inputFiles[i], 'a') as fp:
                        try:
                            fp.write("\n]\n}\n")
                        except Exception as ex:
                            # give up
                            print("Failed to add closing tags to trace file " + inputFiles[i])
                            continue

                    # read the file again
                    rollback = False
                    with open(inputFiles[i], 'r') as fp:
                        try:
                            data = json.load(fp)
                        except Exception as ex:
                            # give up, but need to roll back the changes made to the file
                            rollback = True
                    if (rollback == True):
                        with open(inputFiles[i], 'a') as fp:
                            try:
                                fp.truncate(fsize)
                            except Exception as ex:
                                print("Failed to rollback the changes to trace file " + inputFiles[i])
                        print("Skip invalid trace file " + inputFiles[i])
                        continue
                    else:
                        print("File " + inputFiles[i] + " is modified with proper closing tags added")

                if 'traceEvents' in data:
                    #ofp.write(json.dumps(data['traceEvents']))
                    for e in data['traceEvents']:
                        ofp.write(json.dumps(e))
                        ofp.write(',\n')

        pos = ofp.tell() - 2;	# undo last ',\n'
        ofp.seek(pos, os.SEEK_SET)
        ofp.write('\n]\n')
        ofp.write('}\n')
