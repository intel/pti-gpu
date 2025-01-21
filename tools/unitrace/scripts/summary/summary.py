#!/usr/bin/env -S python3 -OO
import sys
if sys.platform == 'win32': import os; sys.path.append( os.path.dirname( os.path.realpath( __file__ ) ) )

import argparse
import asyncio
from collections import defaultdict
from collections.abc import Iterator
from csv import DictReader
from io import TextIOBase, TextIOWrapper
import json
import logging
from math import fsum
from operator import itemgetter
from pathlib import Path
from statistics import fmean, median
import tarfile


logger = logging.getLogger( __name__ )


# sys.argv = 'summary --tarfile=unitrace_4_mpi_100iter.tgz --schema=LLaMA.json -vvv'.split()
# sys.argv = 'summary --tarfile=unitrace_4_mpi_100iter.tgz --schema=LLaMA.json --json=summary_4_mpi_100iter.json -vv'.split()
# sys.argv = 'summary --tarfile=unitrace_512_256_1_1_200_iters_8ranks.tgz --schema=LLaMA.json -vvv'.split()


parser = argparse.ArgumentParser( description='Process unitrace summary and generate JSON file' )

parser.add_argument( '-v', '--verbose', action='count', default=0,
   help='Show verbose messages' )

parser.add_argument( '--tarfile', '--input', type=Path, required=True,
   help='TAR file with summary outputs from unitrace' )
parser.add_argument( '--schema', type=Path, required=True,
   help='JSON output of `categorize` tool with mapping functions names to categories' )
parser.add_argument( '--aggregator', type=str, choices=('min','max','mean','median','sum','min+1','max-1'), default='mean',
   help='How to aggregate metrics between different summary files from TAR file' )
parser.add_argument( '--skip-empty', action=argparse.BooleanOptionalAction, default=True,
   help='Do not output empty JSON files' )
parser.add_argument( '--json', '--output', type=Path,
   help='Name of output JSON file' )


args = parser.parse_args()


logging.basicConfig( format='[%(levelname)s] <%(name)s.%(funcName)s> %(message)s', level=[ logging.ERROR, logging.WARNING, logging.INFO, logging.DEBUG ][ min( 3, args.verbose ) ] )


def skip_to( labels:list[str], f:TextIOBase ):
   for line in f:
      for index, label in enumerate( labels ):
         if label in line: return index, line
   return -1, None


def skip_empty( f:TextIOBase ):
   for line in f:
      if line.strip(): return line 
   return None


def stop_on_empty( f:TextIOBase ):
   for line in f:
      if not line.strip(): break
      yield line


def csv_from( f:TextIOBase ):
   header = tuple( map( str.strip, skip_empty( f ).strip().split( ',' ) ) )
   return DictReader( stop_on_empty( f ), header, skipinitialspace=True )


def search_tree( tree:dict[str,int|str|dict], tag:str ):
   category = None
   while True:
      category = tree.get( 'category', category )
      if tag:
         if ( key_length := tree.get( 'length', -1 ) ) == -1: return category
         elif len( tag ) < key_length: return category
         else:
            key, tag = tag[:key_length], tag[key_length:]
            if ( next_tree := tree['next'].get( key, None ) ) is not None: tree = next_tree
            else: return category
      else:
         if ( next_nodes := tree.get( 'next', None ) ) is None: return category
         else: return next_nodes['']['category']


def categorize( schema:dict[str,int|str|dict], name:str, default='other' ):
   if   ( category := search_tree( schema['direct' ], name       ) ) is not None: return category
   elif ( category := search_tree( schema['reverse'], name[::-1] ) ) is not None: return category
   else:
      logger.debug( f'U: {name}' )
      return default


def min_p_1( val_iter:Iterator[float|int] ):
   values = sorted( val_iter )
   match len( values ):
      case 1: return values[0]
      case 2: return 0.5 * fsum( values ) if isinstance( values[0], float ) else sum( values ) // 2
      case 0: return 0
      case _: 
         values.sort()
         return values[1]


def max_m_1( val_iter:Iterator[float|int] ):
   values = sorted( val_iter )
   match len( values ):
      case 1: return values[-1]
      case 2: return 0.5 * fsum( values ) if isinstance( values[-1], float ) else sum( values ) // 2
      case 0: return 0
      case _: 
         values.sort()
         return values[-2]


async def aprocess( args ):
   metric_labels = ( 'Time (ns)', 'Calls' )
   metrics:tuple[defaultdict[str,list[tuple[int|float,...]]],...] = ( defaultdict(list), defaultdict(list), defaultdict(list) )
   with tarfile.open( args.tarfile ) as tar:
      for summary in tar:
         if ( bin_file := tar.extractfile( summary ) ) is not None:
            with TextIOWrapper( bin_file ) as f:
               logger.info( f'Processing {summary.name} ...' )
               section = -1
               while ( action_line := skip_to( ( 'API Timing Summary',               # 0
                                                 'Device Timing Summary',            # 1
                                                 '****************************',     # 2
                                                 'Total Execution Time',             # 3
                                                 'Total API Time for L0 backend',    # 4
                                                 'Total Device Time for L0 backend', # 5
                                                 'L0 Backend'                        # 6
                                               ), f ) )[0] >= 0:
                  action, line = action_line
                  match action:
                     case 0 | 1:
                        section = action
                     case 2 if section != 2:
                        section = action
                     case 2 if section == 2:
                        for record in csv_from( f ): metrics[section][record['Function']].append( tuple( int( record[label] ) for label in metric_labels ) )
                        section = -1
                     case 3:
                        label, sep, metric = line.rpartition( ':' )
                        metrics[section]['Wallclock'].append( ( int( metric ), 1 ) )
                     case 4 | 5:
                        label, sep, metric = line.rpartition( ':' )
                        metrics[section]['L0_backend'].append( ( int( metric ), 1 ) )
                     case 6 if section == 0:
                        for record in csv_from( f ): metrics[section][record['Function']].append( tuple( int( record[label] ) for label in metric_labels ) )
                     case 6 if section == 1:
                        for record in csv_from( f ): metrics[section][record['Kernel']].append( tuple( int( record[label] ) for label in metric_labels ) )
            bin_file.close()

   remove_spaces = str.maketrans( '', '', ' \t' )
   with args.schema.open() as f: schema = json.load( f )
   aggregator = { 'min':min, 'max':max, 'mean':fmean, 'median':median, 'sum':fsum, 'min+1':min_p_1, 'max-1':max_m_1 }[args.aggregator]
   extractors = { 'time': itemgetter( 0 ), 'calls': itemgetter( 1 ) }
   output:dict[str,int|float] = {}
   for section in range( 1, 3 ):
      default_category = ( 'host', 'device', 'comm' )[section]
      for name, values in metrics[section].items():
         if schema['ignore_spaces']: name = name.translate( remove_spaces )
         if ( category := categorize( schema, name, default_category ) ):
            for label, getter in extractors.items():
               value = aggregator( map( getter, values ) )
               sub_category = category + '_' + label
               if sub_category in output: output[sub_category] += value
               else                     : output[sub_category]  = value
   if output or not args.skip_empty:
      if args.json is None:
         json.dump( output, sys.stdout, indent=2 )
      else:
         with args.json.open( 'w' ) as f: json.dump( output, f, indent=2 )


action = aprocess( args )

if args.verbose <= 3: asyncio.run( action )
else:
   import cProfile
   import pstats

   logging.getLogger().setLevel( logging.ERROR )
   cProfile.run( 'asyncio.run( action )', f'{__name__}.profile' )
   pstats.Stats( f'{__name__}.profile' ).strip_dirs().sort_stats( 'time' ).print_stats()
