import os
import os.path

from setuptools import Extension, setup
from Cython.Build import cythonize
from Cython.Compiler import Options

extension_args = {}

Options.language_level = 3

# If building Synthizer itself, set additional include and lib directories to the root of the SYnthizer repoisitory.
if 'BUILDING_SYNTHIZER' in os.environ:
    print("Building Synthizer from repository. Adding additional directories.")
    repo_root = os.path.abspath(os.path.join(os.path.split(os.path.abspath(__file__))[0], "../.."))
    print("Repository root is", repo_root)
    if "SYNTHIZER_CI" in os.environ:
        synthizer_build_dir = os.path.join(repo_root, "build_static_release")
        synthizer_lib = "synthizer_static"
    else:
        synthizer_build_dir = os.path.join(repo_root, "build")
        synthizer_lib = "synthizer"
    synthizer_include_dir = os.path.    join(repo_root, "include")
    extension_args = {
        'include_dirs': [synthizer_include_dir],
        'library_dirs': [synthizer_build_dir],
        'libraries': [synthizer_lib],
        'extra_compile_args': [ "/O2" ],
    }

extensions = [
    Extension("synthizer", ["synthizer/synthizer.pyx"], **extension_args),
]

setup(
    name = "synthizer",
    version = "0.7.0",
    ext_modules = cythonize(extensions, language_level=3),
)