# -*- coding: utf-8 -*-
"""
    sphinxcontrib.cmtinc
    ~~~~~~~~~~~~~~~~~~~~~~~

    Extract comments from source files.

    See the README file for details.

    :author: Vilibald W. <vilibald@wvi.cz>
    :license: MIT, see LICENSE for details
"""

import codecs
import re
from docutils import nodes
from sphinx.util.nodes import nested_parse_with_titles
from docutils.statemachine import ViewList
from docutils.parsers.rst import Directive

re_comment = re.compile("^\s?/\*\*\**(.*)$")
re_cmtnext = re.compile("^[/\* | \*]?\**(.*)$")
re_cmtend = re.compile("(.*)(\*/)+$")


class ExtractError(Exception):
    pass


class Extractor(object):
    """
    Main extraction class
    """

    def __init__(self):
        """
        """
        self.content = ViewList("", 'comment')
        self.lineno = 0
        self.is_multiline = False

    def extract(self, source, prefix):
        """
        Process the source file and fill in the content.
        SOURCE is a fileobject.
        """
        for l in source:
            self.lineno = self.lineno + 1
            l = l.strip()
            m = re_comment.match(l)

            if (m):
                self.comment(m.group(1), source, prefix)

    def comment(self, cur, source, prefix):
        """
        Read the whole comment and strip the stars.

        CUR is currently read line and SOURCE is a fileobject
        with the source code.
        """
        self.content.append(cur.strip(), "comment")

        for line in source:
            self.lineno = self.lineno + 1
            # line = line.strip()

            if re_cmtend.match(line):
                if (not self.is_multiline):
                    break
                else:
                    continue

            if line.startswith("/*"):
                if (not self.is_multiline):
                    raise ExtractError(
                        "%d: Nested comments are not supported yet." %
                        self.lineno)
                else:
                    continue

            if line.startswith(".. "):
                self.content.append(line, "comment")
                continue

            if line.startswith("\code"):
                self.is_multiline = True
                continue

            if line.startswith("\endcode"):
                self.is_multiline = False
                continue

            if line.startswith("/"):
                line = "/" + line

            m = re_cmtnext.match(line)
            if m:
#               self.content.append("    " + m.group(1).strip(), "comment")
#               self.content.append("" + m.group(1), "comment")
                self.content.append(prefix + m.group(1), "comment")
                continue

            self.content.append(line.strip(), "comment")

        self.content.append('\n', "comment")


class CmtIncDirective(Directive):
    """
    Directive to insert comments form source file.
    """
    has_content = True
    required_arguments = 1
    optional_arguments = 1
    final_argument_whitespace = True
    option_spec = {}

    def run(self):
        """Run it """
        self.reporter = self.state.document.reporter
        self.env = self.state.document.settings.env
        if self.arguments:
            if self.content:
                return [
                    self.reporter.warning(
                        'include-comment directive cannot have content only '
                        'a filename argument',
                        line=self.lineno)
                ]
            rel_filename, filename = self.env.relfn2path(self.arguments[0])
#            prefix = '' if len(self.arguments) < 2 else re.sub('\'\"', '', self.arguments[1])
            prefix = ''
            self.env.note_dependency(rel_filename)

            extr = Extractor()
            f = None
            try:
                encoding = self.options.get('encoding',
                                            self.env.config.source_encoding)
                codecinfo = codecs.lookup(encoding)
                f = codecs.StreamReaderWriter(
                    open(filename, 'rb'), codecinfo[2], codecinfo[3], 'strict')
                extr.extract(f, prefix)
            except (IOError, OSError):
                return [
                    self.reporter.warning(
                        'Include file %r not found or reading it failed' %
                        filename,
                        line=self.lineno)
                ]
            except UnicodeError:
                return [
                    self.reporter.warning(
                        'Encoding %r used for reading included file %r seems to '
                        'be wrong, try giving an :encoding: option' %
                        (encoding, filename))
                ]
            except ExtractError as e:
                return [
                    self.reporter.warning(
                        'Parsing error in %s : %s' % (filename, str(e)),
                        line=self.lineno)
                ]
            finally:
                if f is not None:
                    f.close()
            self.content = extr.content
            self.content_offset = 0
            # Create a node, to be populated by `nested_parse`.
            node = nodes.paragraph()
            # Parse the directive contents.
            nested_parse_with_titles(self.state, self.content, node)
            return node.children
        else:
            return [
                self.reporter.warning(
                    'include-comment directive needs a filename argument',
                    line=self.lineno)
            ]


def setup(app):
    app.add_directive('include-comment', CmtIncDirective)


if __name__ == '__main__':
    import sys

    if len(sys.argv) < 2:
        print("Usage: cmtinc.py <file.c|cpp|h>")
        exit(1)

    ext = Extractor()
    try:
        with open(sys.argv[1], 'r') as f:
            ext.extract(f,"")
    except ExtractError as e:
        print('Extraction error in external source file %r : %s' %
              (sys.argv[1], str(e)))
    for line in ext.content:
        print(line)
