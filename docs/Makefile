# Makefile for Sphinx documentation
#
# Heavily trimmed from the version used for Flipper

PYTHON = python3
BUILD = sphinx-build

BUILDDIR = build
GENRSTDIR = genrst

# Force rebuilds from scratch for consistency
FORCE = -E

SFLAGS = -d $(BUILDDIR)/doctrees .


.PHONY: clean html

clean:
	rm -rf $(BUILDDIR)/*
	rm -f $(GENRSTDIR)/*

html:
	$(BUILD) $(FORCE) -b html $(SFLAGS) $(BUILDDIR)/html

latex:
	$(BUILD) $(FORCE) -b latex $(SFLAGS) $(BUILDDIR)/latex
