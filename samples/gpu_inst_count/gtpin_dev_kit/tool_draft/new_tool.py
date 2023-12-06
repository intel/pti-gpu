
import os
import sys
import shutil
import re
import argparse
from pathlib import Path

FORCE_REWRITE_TOOL=False
# FORCE_REWRITE_TOOL=True

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
BASE_DIR = os.path.join(SCRIPT_DIR, '..')

DRAFT_TOOL_NAME_CAMEL = 'gtpinToolDraft'
DRAFT_TOOL_NAME_UPPER_CAMEL = 'GtpinToolDraft'
DRAFT_TOOL_NAME_SNAKE = 'gtpin_tool_draft'
DRAFT_TOOL_NAME_UPPER_SNAKE = 'GTPIN_TOOL_DRAFT'
DRAFT_TOOL_FILENAME = 'gtpin_tool_draft_filename'

DRAFT_TOOL_DIR = os.path.join(BASE_DIR, 'tool_draft')
INCLUDE_DIR = DRAFT_TOOL_DIR # os.path.join(BASE_DIR, 'include')
DEFAULT_TOOL_NAME = ''
DEFAULT_DIR = ''


def main(argv):
    print("hell o wolf")
    parser = argparse.ArgumentParser(description='Tool blank creation script')
    parser.add_argument(
        '-d',
        '--directory',
        metavar='PATH',
        default=DEFAULT_DIR,
        dest='directory',
        help='Target directory where tool will be created')
    parser.add_argument(
        '-n',
        '-t',
        '--tool-name',
        metavar='STR',
        default=DEFAULT_TOOL_NAME,
        dest='toolName',
        help='Target directory where tool will be created')
    parser.add_argument(
        '--not-interactive',
        default=True,
        dest='interactive',
        action='store_false',
        help='[Deprecated, no more needed] Work with PVC/DG2')
    args = parser.parse_args(argv)

    if args.directory == DEFAULT_DIR:
        args.directory = os.getcwd()

    if args.interactive and args.toolName == DEFAULT_TOOL_NAME:
        args.toolName = input("Enter new tool name: ")
    if len(args.toolName) <=0:
        print('Please enter valid tool name\n')
        exit()
    args.toolName = re.sub(r'(?<!^)(?=[A-Z])', ' ', args.toolName).lower()
    args.toolName = args.toolName.replace('_',' ')
    toolNameArr = [x.lower() for x in args.toolName.split()]
    if len(toolNameArr) < 1:
        print('Please enter valid tool name\n')
        return
    toolNameUpperCamel = ''.join([(x[0].upper() + (x[1:] if len(x)>1 else '')) for x in toolNameArr[0:]])
    toolNameCamel = toolNameUpperCamel
    toolNameCamel = toolNameCamel[0].lower() +  (toolNameCamel[1:] if len(toolNameCamel)>1 else '')
    toolNameSnake = '_'.join(toolNameArr)
    toolNameUpperSnake = toolNameSnake.upper()
    toolFileName = toolNameSnake
    print(toolNameCamel, toolNameUpperCamel, toolNameSnake, toolNameUpperSnake, toolFileName)

    TARGET_TOOL_DIR = Path(os.path.join(args.directory, toolFileName))

    if args.interactive:
        print(f'Script will create tool blank in "{TARGET_TOOL_DIR}"')
        cont = input("Continue (y/n)?")
        if cont.lower() not in ['y','yes', 'ye']:
            print('Interrupting...\n\n')
            return

    # make directory
    TARGET_TOOL_DIR.mkdir(parents=True, exist_ok=True)
    if args.interactive:
        print(f'Directory "{TARGET_TOOL_DIR}" was created')

    # copy reqiered files with renaming
    FILES_TO_COPY = [
        [DRAFT_TOOL_FILENAME,'.cc', TARGET_TOOL_DIR],
        [DRAFT_TOOL_FILENAME + '_gtpin_launcher','.cc', TARGET_TOOL_DIR],
        [DRAFT_TOOL_FILENAME,'.hpp', TARGET_TOOL_DIR],
        # [build file, 'txt', TARGET_TOOL_DIR]
    ]

    for fil in FILES_TO_COPY:
        dst_dir = os.path.join(
            fil[-1],
        )

        Path(dst_dir).mkdir(parents=True, exist_ok=True)

        src = os.path.join(
            DRAFT_TOOL_DIR,
            ''.join(fil[:-1])
        )
        dst = os.path.join(
            dst_dir,
            ''.join(fil[:-1]).replace(DRAFT_TOOL_FILENAME, toolFileName)
        )

        # continue
        if os.path.exists(dst):
            if FORCE_REWRITE_TOOL:
                if not os.path.isfile(dst):
                    print(f'Target tool path ({dst}) already exists, and not a file')
                    exit()
                os.remove(dst)
            else:
                print(f'Target tool path ({dst}) already exists, exiting')
                exit()

        # copy file and replace tool name
        with open(src, 'r') as f :
            rows = f.read()
        rows = rows.\
            replace(DRAFT_TOOL_FILENAME, toolFileName).\
            replace(DRAFT_TOOL_NAME_CAMEL, toolNameCamel).\
            replace(DRAFT_TOOL_NAME_UPPER_CAMEL, toolNameUpperCamel).\
            replace(DRAFT_TOOL_NAME_SNAKE, toolNameSnake).\
            replace(DRAFT_TOOL_NAME_UPPER_SNAKE, toolNameUpperSnake)

        with open(dst, 'w') as f:
            f.write(rows)

        print(f'File "{src}" copied to "{dst}"')


    print(f'\"{args.toolName}\" tool was created\n\n')

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:] if len(sys.argv) > 0 else []))