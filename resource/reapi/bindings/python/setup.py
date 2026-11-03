import os
from setuptools import setup, Extension, find_packages
from Cython.Build import cythonize

base_dir = os.path.abspath(os.path.dirname(__file__))

# We need headers for the Cython compilation
bindings_dir = os.path.abspath(os.path.join(base_dir, "../"))
reapi_c_dir = os.path.abspath(os.path.join(base_dir, "../../c"))
here = os.path.dirname(os.path.abspath(__file__))
root_dir = here
for _ in range(4):
    root_dir = os.path.dirname(root_dir)

readme = os.path.join(here, "README.md")
try:
    with open("README.md", "r", encoding="utf-8") as fh:
        long_description = fh.read()
except FileNotFoundError:
    long_description = "Python bindings for the Flux Scheduler (flux-sched)."

extensions = [
    Extension(
        "flux_sched.reapi_cli",
        sources=["flux_sched/reapi_cli.pyx"],
        library_dirs=[bindings_dir],
        include_dirs=[reapi_c_dir, bindings_dir, root_dir],
        libraries=["reapi_cli", "flux-core"],
        language="c++",
        extra_compile_args=["-std=c++11"],
    ),
    Extension(
        "flux_sched.reapi_module",
        sources=["flux_sched/reapi_module.pyx"],
        library_dirs=[bindings_dir],
        include_dirs=[reapi_c_dir, bindings_dir, root_dir],
        libraries=["reapi_module", "flux-core"],
        language="c++",
        extra_compile_args=["-std=c++11"],
    ),
]

setup(
    name="flux-sched",
    version="0.0.0",
    packages=find_packages(),
    ext_modules=cythonize(extensions),
    install_requires=[],
    extras_require={
        "reapi-module": ["flux-python"],
    },
    description="Python bindings for the Flux resource manager framework scheduler",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Flux Framework Developers",
    author_email="flux-discuss@llnl.gov",
    maintainer="Vanessa Sochat",
    url="https://github.com/flux-framework/flux-sched",
    project_urls={
        "Documentation": "https://flux-framework.readthedocs.io/",
        "Source": "https://github.com/flux-framework/flux-sched",
        "Tracker": "https://github.com/flux-framework/flux-sched/issues",
    },
    zip_safe=False,
    classifiers=[
        "Intended Audience :: Science/Research",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU Lesser General Public License v3 or later (LGPLv3+)",
        "Programming Language :: C++",
        "Programming Language :: Python",
        "Topic :: Software Development",
        "Topic :: Scientific/Engineering",
        "Operating System :: Unix",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
    ],
    keywords=[
        "hpc",
        "flux",
        "fluxion",
        "scheduler",
        "resource-manager",
        "distributed-computing",
        "graph-scheduling",
    ],
)
