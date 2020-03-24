Shortcake Hub Module Documentation
==================================

This directory contains the documentation for the "hub" module that
communicates with the "Shortcake" Hat.  The documentation is here as
ReStructured Text source files, together with a Makefile for building
it into HTML.  Other builds can be added as required.

Building the Documentation
--------------------------

Getting the documentation converted from ReStructured Text to your
preferred format requires the sphinx package, and optionally (for
RTD-styling) sphinx_rtd_theme package.  To install them:

    pip install sphinx
    pip isntall sphinx_rtd_theme

then to build the documentation as HTML:

    make html
