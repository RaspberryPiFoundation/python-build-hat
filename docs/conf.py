#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
from collections import OrderedDict

sys.path.insert(0, os.path.abspath('.'))

# Need this to correctly find the docstrings for python modules
sys.path.insert(0, os.path.abspath('..'))

# Common strings
PORT = 'hub'
VERSION = 'latest'

tags.add('port_' + PORT)
ports = OrderedDict((
    ('hub', 'the Build HAT'),
))

# The members of the html_context dict are available inside
# topindex.html

URL_PATTERN = '//en/%s/%s'
HTML_CONTEXT = {
    'port': PORT,
    'port_name': ports[PORT],
    'port_version': VERSION,
    'all_ports': [ (port_id, URL_PATTERN % (VERSION, port_id))
                   for port_id, port_name in ports.items() ],
    'all_versions': [ VERSION, URL_PATTERN % (VERSION, PORT) ],
    'downloads': [ ('PDF', URL_PATTERN % (VERSION,
                                          'python-%s.pdf' % PORT)) ]
}


# Specify a custom master document based on the port name
master_doc = PORT + "_index"

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.doctest',
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'sphinx.ext.coverage',
    'sphinx_selective_exclude.modindex_exclude',
    'sphinx_selective_exclude.eager_only',
    'sphinx_selective_exclude.search_auto_exclude',
    'sphinxcontrib.cmtinc-buildhat',
]


# Add any paths that contain templates here, relative to this directory.
templates_path = ['templates']

# The suffix of source filenames.
source_suffix = '.rst'

# General information about the project.
project = 'Raspberry Pi Build HAT'
copyright = '''2020-2025 - Raspberry Pi Foundation;
2017-2020 - LEGO System A/S - Aastvej 1, 7190 Billund, DK.'''

# The version info for the project you're documenting, acts as replacement for
# |version| and |release|, also used in various other places throughout the
# built documents.
#
# We don't follow "The short X.Y version" vs "The full version, including alpha/beta/rc tags"
# breakdown, so use the same version identifier for both to avoid confusion.
with open(os.path.join(os.path.dirname(__file__),'../VERSION')) as versionf:
    version = versionf.read().strip()

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
exclude_patterns = ['build', '.venv']
exclude_patterns += ['pending']

# The reST default role (used for this markup: `text`) to use for all
# documents.
default_role = 'any'

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# Global include files. Sphinx docs suggest using rst_epilog in preference
# of rst_prolog, so we follow. Absolute paths below mean "from the base
# of the doctree".
rst_epilog = """
.. include:: /templates/replace.inc
"""

# -- Options for HTML output ----------------------------------------------

# on_rtd is whether we are on readthedocs.org
on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

if not on_rtd:  # only import and set the theme if we're building docs locally
    try:
        import sphinx_rtd_theme
        extensions.append("sphinx_rtd_theme")
        html_theme = 'sphinx_rtd_theme'
        html_theme_path = [sphinx_rtd_theme.get_html_theme_path(), '.']
    except:
        html_theme = 'default'
        html_theme_path = ['.']
else:
    html_theme_path = ['.']

# The name of an image file (within the static path) to use as favicon of the
# docs.  This file should be a Windows icon file (.ico) being 16x16 or 32x32
# pixels large.
html_favicon = 'build/html/_static/favicon.ico'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['static']

# If not '', a 'Last updated on:' timestamp is inserted at every page bottom,
# using the given strftime format.
html_last_updated_fmt = '%d %b %Y'

# Additional templates that should be rendered to pages, maps page names to
# template names.
html_additional_pages = {"index": "topindex.html"}

# Output file base name for HTML help builder.
htmlhelp_basename = 'BuildHATdoc'
