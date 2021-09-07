Build HAT Module Documentation
==============================

This directory contains the documentation for the "buildhat" module
that communicates with the Raspberry Pi Build HAT.  The documentation
is here as ReStructured Text source files, together with a Makefile
for building it into HTML.  Other builds can be added as required.

Building the Documentation
--------------------------

Getting the documentation converted from ReStructured Text to your
preferred format requires the sphinx package, and optionally (for
RTD-styling) sphinx\_rtd\_theme package.  To install them:

```
pip install sphinx
pip install sphinx_rtd_theme
```

If using asdf:

```
asdf reshim python
```

Then to build the documentation as HTML:

```
make html
```
