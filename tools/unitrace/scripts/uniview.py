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
try:
    import analyzeperfmetrics as apm
except ImportError:
    from metrics import analyzeperfmetrics as apm


def ParseArguments():
    argparser = argparse.ArgumentParser(description = "View trace and hardware metrics in https://ui.perfetto.dev")
    argparser.add_argument('-t', '--trace', required = True, help = "trace file in JSON format")
    argparser.add_argument('-f', '--config', help = "metric view config file ")
    argparser.add_argument('-s', '--shaderdump', help = "shader dump folder for stall analysis")
    argparser.add_argument('-m', '--metrics', help = "hardware performance metrics file in CSV format")
    argparser.add_argument('-n', '--numtopstalls', type = int, default = 10, help = "number of top most expensive stalls of each type to report for stall analysis(10 default, -1 unlimited)")
    argparser.add_argument('-g', '--demangler', help = "symbol demangler if c++filt is not available")

    return argparser.parse_args()

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

def main():
    args = ParseArguments();

    if args.trace is None:
        print(f'Trace file is missing')
        return 1;
    if not os.path.exists(args.trace):
        print(f'Trace file {args.trace} is not found')
        return 1;

    eustall = True
    if (args.metrics is not None):
        if not os.path.exists(args.metrics):
            print(f'Metrics file {args.metrics} is not found')
            return 1

        with open(args.metrics, 'r') as fp:
            for num, line in enumerate(fp):
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

    LoadTrace(args.trace)

    if args.metrics is not None:
        https = False
        with open(args.trace, 'r') as fp:
            for num, line in enumerate(fp):
                if ("https://" in line):
                    https = True
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
            options.extend(['-p', str(args.metrics)])
        else:
            options.extend(['-q', str(args.metrics)])

        apm.main(apm.ParseArguments(options))

if __name__ == '__main__':
    sys.exit(main())
