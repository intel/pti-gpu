#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================


#!/usr/bin/env python3

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
            if (os.stat(inputFiles[i]).st_size !=0):
                with open(inputFiles[i], 'r') as fp:
                    try:
                        data = json.load(fp)
                    except Exception as ex:
                        print("Skip invalid trace file " + inputFiles[i])
                        continue

                    #if data['traceEvents']:
                    if 'traceEvents' in data:
                        #ofp.write(json.dumps(data['traceEvents']))
                        for e in data['traceEvents']:
                            #print(e)
                            ofp.write(json.dumps(e))
                            ofp.write(',\n')

        pos = ofp.tell() - 2;	# undo last ',\n'
        ofp.seek(pos, os.SEEK_SET)
        ofp.write('\n]\n')
        ofp.write('}\n')
