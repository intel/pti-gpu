#!/usr/bin/env python3
#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================



import os
import argparse
import json

def ParseCommandLineArgs():
    parser = argparse.ArgumentParser(description = 'Merge unitrace result files')
    parser.add_argument('inputFiles', nargs = '+', help = 'list of files to merge')
    parser.add_argument('-o', '--outputFile', default = 'unitrace.all.json', help = 'output file')

    args = parser.parse_args()

    return (args.inputFiles, args.outputFile)

if __name__ == "__main__":

    inputFiles, outputFile = ParseCommandLineArgs()

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
