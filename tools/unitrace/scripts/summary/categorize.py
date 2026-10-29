#!/usr/bin/env -S python3 -OO
import sys
if sys.platform == 'win32': import os; sys.path.append( os.path.dirname( os.path.realpath( __file__ ) ) )

import argparse
import asyncio
from collections import defaultdict
import configparser
from configparser import ConfigParser
import json
import logging
from pathlib import Path


logger = logging.getLogger( __name__ )


# sys.argv = 'categorize --ini=LLaMA.ini --json=LLaMA.json -v'.split()


def build_search_tree( tags:list[tuple[str,str,bool]] ):
   # No tags of this kind (e.g. no "ends with" section): a tree that matches nothing.
   if not tags: return {}
   key_length = min( len( tag[0] ) for tag in tags )
   next_nodes:dict[str,int|str|dict] = {}
   tree:dict[str,int|str|dict] = { 'length':key_length, 'next':next_nodes }
   next_tags:dict[str,list[tuple[str,str,bool]]] = defaultdict(list)
   for tag, category, is_exact in tags:
      key, next_tag = tag[:key_length], tag[key_length:]
      if   next_tag: next_tags[key].append( ( next_tag, category, is_exact ) )
      elif is_exact: next_nodes[key] = { 'next': { '': { 'category': category } } }
      else         : next_nodes[key] = { 'category': category }
   for key, tags in next_tags.items():
      sub_tree = build_search_tree( tags )
      if ( node := next_nodes.get( key, None ) ) is None: next_nodes[key] = sub_tree
      else:
         if 'next' in node and 'next' in sub_tree: sub_tree['next'].update( node['next'] )
         node.update( sub_tree )
   return tree


def build_tree_from_ini( ini_path, remove_tag_spaces ):
   """Build the category search trees from a categories INI file.

   ini_path: the INI path; sections are "<category>[.<sub>] if <verb>", keys are
   tags, ":=" is the only delimiter and "#" starts comments.
   remove_tag_spaces: remove spaces and tabs from the tags.
   Returns {'ignore_spaces', 'direct', 'reverse'}: "ends with" tags go reversed
   into the reverse tree, "equals to" tags are exact, the rest are prefixes.
   A missing file is ignored by ConfigParser.read (empty trees).
   Raises configparser.Error (e.g. ParsingError, DuplicateSectionError) on
   malformed INI; the AttributeError CPython 3.12 and earlier raise on a
   continuation line after a key without a value is re-raised as ParsingError.
   """
   remove_spaces = str.maketrans( '', '', ' \t' )
   assign_op = ':='
   config = ConfigParser( allow_no_value=True, delimiters=( assign_op, ), comment_prefixes=( '#', ) )
   config.optionxform = str
   try:
      config.read( ini_path )
   except AttributeError as ex:
      # CPython's configparser (3.12 and earlier) fails this way on a continuation line
      # after a key without a value; report it as the parse error it is.
      raise configparser.ParsingError( str( ini_path ) ) from ex
   direct_search:list[tuple[str,str,bool]] = []
   reverse_search:list[tuple[str,str,bool]] = []
   for name, section in config.items():
      if name != config.default_section:
         category, sep, verb = name.partition( ' if ' )
         category, sep, sub_category = category.strip().partition( '.' )
         verb = verb.translate( remove_spaces )
         if verb == 'endswith':
            for tag1, tag2 in section.items():
               tag = tag1 if tag2 is None else tag1 + assign_op + tag2
               if remove_tag_spaces: tag = tag.translate( remove_spaces )
               reverse_search.append( ( tag[::-1], category, False ) )
         else:
            is_exact = verb == 'equalsto'
            for tag1, tag2 in section.items():
               tag = tag1 if tag2 is None else tag1 + assign_op + tag2
               if remove_tag_spaces: tag = tag.translate( remove_spaces )
               direct_search.append( ( tag, category, is_exact ) )
   return { 'ignore_spaces': remove_tag_spaces, 'direct': build_search_tree( direct_search ), 'reverse': build_search_tree( reverse_search ) }


async def aprocess( args ):
   tree = build_tree_from_ini( args.ini, args.remove_spaces )
   if logger.isEnabledFor( logging.INFO ):
      indent, separators = 3, None
   else:
      indent, separators = None, ( ',', ':' )
   with args.json.open( 'w' ) as f: json.dump( tree, f, indent=indent, separators=separators )


def main():
   """Command-line entry point of the categorize tool.

   Parses --ini, --remove-spaces/--no-remove-spaces, --json and -v, configures
   logging from the -v count and runs aprocess, which writes the search trees
   from build_tree_from_ini to --json (indented with -v and above).
   With more than three -v the run is profiled with cProfile instead.
   argparse exits on invalid arguments.
   """
   parser = argparse.ArgumentParser( description='Process INI file with categories description and generate JSON file to use with `summary` tool' )

   parser.add_argument( '-v', '--verbose', action='count', default=0,
      help='Show verbose messages' )

   parser.add_argument( '--ini', '--input', type=Path, required=True,
      help='INI file with categories description' )
   parser.add_argument( '--remove-spaces', action=argparse.BooleanOptionalAction, default=True,
      help='Remove spaces from tags' )
   parser.add_argument( '--json', '--output', type=Path, required=True,
      help='Name of output JSON file to be used with `summary` tool' )

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
