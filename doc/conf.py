###############################################################
# Copyright 2020 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

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
import os
import sys
from manpages import man_pages
import docutils.nodes

# -- Project information -----------------------------------------------------

project = "flux-sched"
copyright = """Copyright 2022 Lawrence Livermore National Security, LLC and Flux developers.

SPDX-License-Identifier: LGPL-3.0"""

# -- General configuration ---------------------------------------------------

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

master_doc = "index"
source_suffix = ".rst"

extensions = ["sphinx.ext.intersphinx", "sphinx.ext.napoleon", "domainrefs"]

domainrefs = {
    "linux:man1": {
        "text": "%s(1)",
        "url": "https://linux.die.net/man/1/%s",
    },
    "linux:man2": {
        "text": "%s(2)",
        "url": "https://linux.die.net/man/2/%s",
    },
    "linux:man3": {
        "text": "%s(3)",
        "url": "https://linux.die.net/man/3/%s",
    },
    "linux:man7": {
        "text": "%s(7)",
        "url": "https://linux.die.net/man/7/%s",
    },
    "linux:man8": {
        "text": "%s(8)",
        "url": "https://linux.die.net/man/8/%s",
    },
    "core:man1": {
        "text": "%s(1)",
        "url": "https://flux-framework.readthedocs.io/projects/flux-core/en/latest/man1/%s.html",
    },
    "core:man3": {
        "text": "%s(3)",
        "url": "https://flux-framework.readthedocs.io/projects/flux-core/en/latest/man3/%s.html",
    },
    "core:man5": {
        "text": "%s(5)",
        "url": "https://flux-framework.readthedocs.io/projects/flux-core/en/latest/man5/%s.html",
    },
    "core:man7": {
        "text": "%s(7)",
        "url": "https://flux-framework.readthedocs.io/projects/flux-core/en/latest/man7/%s.html",
    },
}

# Disable "smartquotes" to avoid things such as turning long-options
#  "--" into en-dash in html output, which won't make much sense for
#  manpages.
smartquotes = False

# -- Setup for Sphinx API Docs -----------------------------------------------

# Workaround since sphinx does not automatically run apidoc before a build
# Copied from https://github.com/readthedocs/readthedocs.org/issues/1139

script_dir = os.path.normpath(os.path.dirname(__file__))
py_bindings_dir = os.path.normpath(os.path.join(script_dir, "../src/bindings/python/"))

# Make sure that the python bindings are in PYTHONPATH for autodoc
sys.path.insert(0, py_bindings_dir)


def man_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    section = int(name[-1])
    page = None
    for man in man_pages:
        if man[1] == text and man[4] == section:
            page = man[0]
            break
    if page == None:
        page = "man7/flux-undocumented"
        section = 7

    node = docutils.nodes.reference(
        rawsource=rawtext,
        text=f"{text}({section})",
        refuri=f"../{page}.html",
        **options,
    )
    return [node], []


# launch setup
def setup(app):
    for section in [3, 5, 8]:
        app.add_role(f"man{section}", man_role)


napoleon_google_docstring = True

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "sphinx_rtd_theme"

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
# html_static_path = ['_static']

# -- Options for Intersphinx -------------------------------------------------

intersphinx_mapping = {
    "core": (
        "https://flux-framework.readthedocs.io/projects/flux-core/en/latest/",
        None,
    ),
    "rfc": (
        "https://flux-framework.readthedocs.io/projects/flux-rfc/en/latest/",
        None,
    ),
    "workflow-examples": (
        "https://flux-framework.readthedocs.io/projects/flux-workflow-examples/en/latest/",
        None,
    ),
}
