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
from sphinx.util import logging

logger = logging.getLogger(__name__)

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

import shutil

try:
    import breathe  # noqa: F401

    have_breathe = True
except ImportError:
    have_breathe = False
    logger.warning("breathe not found. C/C++ API documentation will not be available.")
    logger.warning("Install breathe with: pip install breathe")

have_doxygen = shutil.which("doxygen") is not None
if not have_doxygen:
    logger.warning("doxygen not found. C/C++ API documentation will not be available.")

# The C/C++ API pages need both the breathe extension and the doxygen binary
# (breathe reads the XML that doxygen generates).  Only register breathe when
# both are present: registering it without doxygen leaves the
# `.. doxygenfile::` directives live but with no XML to read, which can fail
# the build.  When either is missing, leave breathe unregistered so those
# directives degrade to harmless "unknown directive" warnings instead.
BREATHE_AVAILABLE = have_breathe and have_doxygen
if BREATHE_AVAILABLE:
    extensions.append("breathe")

# Suppress warnings
suppress_warnings = [
    "ref.unknown",
    "duplicate_declaration",
    "app.add_role",
    "unknown_role_name",
    "ref.ref",
]

# -- Breathe configuration (Doxygen integration) -----------------------------

if BREATHE_AVAILABLE:
    # Determine build directory
    build_dir = os.environ.get("SPHINX_BUILDDIR", "_build")
    doxygen_xml_dir = os.path.join(build_dir, "doxygen/xml")

    breathe_projects = {
        "flux-sched": doxygen_xml_dir
    }
    breathe_default_project = "flux-sched"
    # Only show documented members - this prevents stubs for undocumented items
    breathe_default_members = ()
    breathe_domain_by_extension = {
        "hpp": "cpp",
        "h": "cpp",
        "cpp": "cpp",
        "c": "c",
    }
    breathe_show_include = True
    breathe_show_enumvalue_initializer = True
    # Suppress warnings for undocumented enum values
    breathe_show_define_initializer = False
    # Don't show enum values without descriptions
    breathe_separate_member_pages = False

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


def run_doxygen():
    """Run Doxygen to generate XML for Breathe."""
    import subprocess
    import shutil

    # Skip if breathe is not available
    if not BREATHE_AVAILABLE:
        logger.info("Skipping Doxygen (breathe not available)")
        return

    # Check if we need to run doxygen (e.g., on Read the Docs)
    read_the_docs_build = os.environ.get("READTHEDOCS", None) == "True"

    # Get paths relative to this config file
    conf_dir = os.path.dirname(os.path.abspath(__file__))
    source_dir = os.path.dirname(conf_dir)  # Parent of doc/ directory
    build_dir = os.environ.get("SPHINX_BUILDDIR", os.path.join(conf_dir, "_build"))
    doxygen_output = os.path.join(build_dir, "doxygen")
    doxygen_xml = os.path.join(doxygen_output, "xml")

    # Skip if doxygen output already exists (unless on RTD)
    if os.path.exists(os.path.join(doxygen_xml, "index.xml")) and not read_the_docs_build:
        logger.info(f"Doxygen XML already exists at {doxygen_xml}, skipping...")
        return

    # Check if doxygen is available
    if shutil.which("doxygen") is None:
        logger.warning("doxygen not found. Skipping API documentation generation.")
        logger.warning("Install doxygen to generate C++ API documentation.")
        return

    logger.info(f"Running Doxygen to generate XML at {doxygen_xml}...")

    # Create a temporary Doxyfile from Doxyfile.in with substitutions
    doxyfile_in = os.path.join(conf_dir, "Doxyfile.in")
    doxyfile_tmp = os.path.join(build_dir, "Doxyfile")

    # Create build directory if it doesn't exist
    os.makedirs(build_dir, exist_ok=True)

    # Read template and substitute variables
    with open(doxyfile_in, "r") as f:
        content = f.read()

    # Perform CMake-like substitutions
    content = content.replace("@PROJECT_VERSION@", "latest")
    content = content.replace("@DOXYGEN_OUTPUT_DIR@", doxygen_output)
    content = content.replace("@CMAKE_SOURCE_DIR@", source_dir)

    # Write temporary Doxyfile
    with open(doxyfile_tmp, "w") as f:
        f.write(content)

    # Run Doxygen
    try:
        result = subprocess.run(
            ["doxygen", doxyfile_tmp],
            cwd=conf_dir,
            check=True,
            capture_output=True,
            text=True
        )
        logger.info("Doxygen completed successfully")
        if result.stdout:
            logger.info(result.stdout)
    except subprocess.CalledProcessError as e:
        logger.warning(f"Doxygen failed with return code {e.returncode}")
        if e.stdout:
            logger.warning(e.stdout)
        if e.stderr:
            logger.warning(e.stderr)
        logger.warning("Continuing without API documentation...")
    except Exception as e:
        logger.warning(f"Failed to run Doxygen: {e}")
        logger.warning("Continuing without API documentation...")


# launch setup
def setup(app):
    # Run Doxygen before building
    run_doxygen()

    # Add custom roles
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
}
