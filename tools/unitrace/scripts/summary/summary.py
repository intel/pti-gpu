#!/usr/bin/env -S python3 -OO
import sys
if sys.platform == 'win32': import os; sys.path.append( os.path.dirname( os.path.realpath( __file__ ) ) )

import argparse
import asyncio
from collections import defaultdict
from collections.abc import Iterator
from csv import DictReader, Error as CsvError
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


class SummaryFormatError( ValueError ):
   """A summary file that does not have the layout unitrace writes.

   Raised by csv_from (table without a header), field (row without a column) and
   to_int (value that is not an integer). A ValueError subclass; read_summary_tar
   catches it and skips the archive member with a warning.
   """


def csv_from( f:TextIOBase ):
   if ( header_line := skip_empty( f ) ) is None: raise SummaryFormatError( 'table without a header' )
   header = tuple( map( str.strip, header_line.strip().split( ',' ) ) )
   return DictReader( stop_on_empty( f ), header, skipinitialspace=True )


def field( record:dict, name:str ):
   """Return the value of column name in a CSV record.

   record: a dict from csv.DictReader. name: the column name.
   Returns the value as a string.
   Raises SummaryFormatError("row without <name>") if the column is missing or
   its value is None (a row shorter than the header).
   """
   if ( value := record.get( name ) ) is None: raise SummaryFormatError( f'row without {name}' )
   return value


def to_int( text ):
   """Convert a summary value to int.

   text: the string (or other value) read from the summary.
   Returns int(text).
   Raises SummaryFormatError("not a number: ...") instead of the TypeError or
   ValueError int() raises, e.g. for None or non-numeric text.
   """
   try: return int( text )
   except ( TypeError, ValueError ): raise SummaryFormatError( f'not a number: {text!r}' ) from None


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
         # The name ends here: the exact match if this node has one, else the
         # category matched so far (a node may only have longer keys).
         if ( next_nodes := tree.get( 'next', None ) ) is None: return category
         else: return next_nodes.get( '', {} ).get( 'category', category )


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


SUMMARY_LABELS = ( 'API Timing Summary',               # 0
                   'Device Timing Summary',            # 1
                   '****************************',     # 2
                   'Total Execution Time',             # 3
                   'Total API Time for L0 backend',    # 4
                   'Total Device Time for L0 backend', # 5
                   'L0 Backend' )                      # 6


def new_metrics():
   """Create an empty metrics container.

   Returns a tuple of three defaultdict(list), one per section: 0 API (host),
   1 device, 2 communication. Each maps a function or kernel name to a list of
   (time_ns, calls) tuples. Used by read_summary_tar for the total and for each
   archive member.
   """
   metrics:tuple[defaultdict[str,list[tuple[int|float,...]]],...] = ( defaultdict(list), defaultdict(list), defaultdict(list) )
   return metrics


def parse_summary( f:TextIOBase, metrics ):
   """Parse one unitrace summary text file into metrics.

   f: the text stream of the summary. metrics: a container from new_metrics,
   appended to in place: per-function/kernel (Time (ns), Calls) rows of the API,
   device and communication tables, and "Wallclock" / "L0_backend" totals.
   Returns None. Lines without a known section label are skipped.
   Raises SummaryFormatError for a table without a header, a row missing the
   Function/Kernel, Time (ns) or Calls column, or a value that is not an integer.
   Called by read_summary_tar, which discards the partial result on error.
   """
   metric_labels = ( 'Time (ns)', 'Calls' )
   row = lambda record, name: ( field( record, name ), tuple( to_int( field( record, label ) ) for label in metric_labels ) )
   section = -1
   while ( action_line := skip_to( SUMMARY_LABELS, f ) )[0] >= 0:
      action, line = action_line
      match action:
         case 0 | 1:
            section = action
         case 2 if section != 2:
            section = action
         case 2 if section == 2:
            for record in csv_from( f ):
               name, values = row( record, 'Function' )
               metrics[section][name].append( values )
            section = -1
         case 3:
            label, sep, metric = line.rpartition( ':' )
            metrics[section]['Wallclock'].append( ( to_int( metric ), 1 ) )
         case 4 | 5:
            label, sep, metric = line.rpartition( ':' )
            metrics[section]['L0_backend'].append( ( to_int( metric ), 1 ) )
         case 6 if section == 0:
            for record in csv_from( f ):
               name, values = row( record, 'Function' )
               metrics[section][name].append( values )
         case 6 if section == 1:
            for record in csv_from( f ):
               name, values = row( record, 'Kernel' )
               metrics[section][name].append( values )


def read_summary_tar( tarfile_path ):
   """Read every unitrace summary in a tar archive and merge their metrics.

   tarfile_path: path of the tar file (any compression tarfile.open detects).
   Returns the merged container from new_metrics. Each regular member is parsed
   on its own with parse_summary; a member raising SummaryFormatError,
   UnicodeDecodeError or csv.Error (e.g. a field above csv.field_size_limit()) is
   skipped with a warning and contributes nothing.
   Non-file members are ignored. Errors opening or reading the archive itself
   (tarfile.TarError, OSError) propagate. Called by aprocess.
   """
   metrics = new_metrics()
   with tarfile.open( tarfile_path ) as tar:
      for summary in tar:
         # Only regular files: extractfile() follows a link, and raises KeyError for
         # one whose target is not in the archive.
         if not summary.isfile(): continue
         if ( bin_file := tar.extractfile( summary ) ) is not None:
            with TextIOWrapper( bin_file ) as f:
               logger.info( f'Processing {summary.name} ...' )
               # Parsed on its own, so a malformed file contributes nothing.
               member = new_metrics()
               try:
                  parse_summary( f, member )
               except ( SummaryFormatError, UnicodeDecodeError, CsvError ) as ex:
                  logger.warning( f'Skipping {summary.name}: not a unitrace summary ({ex})' )
                  continue
               for section, values in zip( metrics, member ):
                  for name, rows in values.items(): section[name].extend( rows )
   return metrics


async def aprocess( args ):
   metrics = read_summary_tar( args.tarfile )
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


def main():
   """Command-line entry point of the summary tool.

   Parses --tarfile, --schema (JSON from categorize), --aggregator, --skip-empty,
   --json and -v, configures logging from the -v count and runs aprocess, which
   writes the per-category time and call totals as JSON to --json or stdout.
   With more than three -v the run is profiled with cProfile instead.
   argparse exits on invalid arguments.
   """
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

   action = aprocess( args )

   if args.verbose <= 3: asyncio.run( action )
   else:
      import cProfile
      import pstats

      logging.getLogger().setLevel( logging.ERROR )
      cProfile.runctx( 'asyncio.run( action )', globals(), locals(), f'{__name__}.profile' )
      pstats.Stats( f'{__name__}.profile' ).strip_dirs().sort_stats( 'time' ).print_stats()


if __name__ == '__main__':
   main()
