# This script vendors Synthizer by copying the minimal set of files to build
# the C library.  It is used to produce copies of Synthizer that exclude everything unnecessary, like HRTF datasets.
# Run from the root of the repo, passing a path to a vendoring directory.
import glob
import itertools
import os
import os.path
import shutil
import sys

if len(sys.argv) != 2:
    print("Usage: {} <output-path>".format(sys.argv[0]))
    sys.exit(1)

dest = os.path.abspath(sys.argv[1])
if os.path.exists(dest):
    shutil.rmtree(sys.argv[1])

this_dir = os.path.split(os.path.abspath(__file__))[0]
patterns = [os.path.join(this_dir, "**", "*.{}".format(i)) for i in ["c", "h", "cpp", "hpp", "txt", "cmake", "in", "cc"]]
files = itertools.chain(*[glob.iglob(pat, recursive=True) for pat in patterns])

forbidden_prefixes = [".git", "build", os.path.join("bindings", "python")]
forbidden_prefixes = [os.path.join(this_dir, i) for i in forbidden_prefixes]

def check_path(p):
    for i in forbidden_prefixes:
        if p.startswith(i):
            return False
    return True

# Don't go lazy here, or iglob recurses into the output if the vendored copy is under the repo root, as sometimes
# happens when testing.
files = [i for i in files if check_path(i)]

for f in files:
    relpath = os.path.relpath(f, this_dir)
    outpath = os.path.join(dest, relpath)
    outdir = os.path.split(outpath)[0]
    os.makedirs(outdir, exist_ok=True)
    shutil.copyfile(f, outpath)
