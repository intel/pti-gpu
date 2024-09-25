# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'PTI Library'
copyright = '2024, Intel Corporation'
author = 'Intel Corporation'

# The full version, including alpha/beta/rc tags
versionfile = open('../../../VERSION')
release = versionfile.readline()

# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.doctest',
    'sphinx.ext.mathjax',
    'sphinx.ext.viewcode',
    'sphinx.ext.imgmath', 
    'sphinx.ext.todo',
    'breathe'
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_book_theme'

html_theme_options = {
    'repository_url': 'https://github.com/intel/pti-gpu',
    'path_to_docs': 'docs/sphinx/source',
    'use_issues_button': True,
    'use_edit_page_button': True,
    'repository_branch': 'master',
    'logo': {
        'text': 'PTI Documentation',
    }
}
html_logo = '_static/oneAPI-rgb-rev-100.png'
html_favicon = '_static/favicons.png'


# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']
html_css_files = ['custom.css']

# Syntax highlighting for the :: directive
highlight_language = 'c' 

# -- Extension configuration -------------------------------------------------

import subprocess
subprocess.call('make clean', shell=True)
subprocess.call('cd ../../doxygen ; doxygen ', shell=True)

breathe_projects = { "ptilib": "../../doxygen/build/xml/" }
breathe_default_project = "ptilib"
#exhale_args = {
#   "containmentFolder":    "./api",
#    "rootFileName":         "library_root.rst",
#    "afterTitleDescription": "The library to build performance tools",
#    "doxygenStripFromPath": "..",
#    "rootFileTitle":        "PTI Library API"
#}
