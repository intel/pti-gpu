#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

#!/usr/bin/env python3

import os
import argparse

def ParseArguments():
    argparser = argparse.ArgumentParser(description = "Add instruction pointers in GPU assembly")
    argparser.add_argument('-o', '--output', required = True, help = "output .asm file with instruction pointers")
    argparser.add_argument('input', help = '.asm file withour instruction pointers')
    
    return argparser.parse_args()

def main(args):
    if (os.path.isfile(args.input) == False):
        print("File " + args.input + " does not exist or cannot be opened")
        return

    if (os.stat(args.input).st_size ==0):
        print("File " + args.input + " is empty")
        return

    ip = 0
    with open(args.input, "r") as inf:
        with open(args.output, "w") as outf:
            for row in inf:
                if ((row.startswith("//") == False) and ("//" in row)):
                    outf.write("/* [" + str('{:08X}'.format(ip)) + "] */ " + row)
                    if ("Compacted" in row):
                        ip = ip + 0x8
                    else:
                        ip = ip + 0x10
                else:
                    outf.write(row)
                
if __name__=="__main__":
    main(ParseArguments())
