from distutils.core import setup, Extension
from Cython.Build import cythonize

# list of all source files for building the lib
_src = [
    "arrayi.c",
    "arrayset.c",
    "arrayu.c",
    "coord2d.c",
    "cvrp.c",
    "evol.c",
    "hash.c",
    "listu.c",
    "listx.c",
    "matrixd.c",
    "matrixu.c",
    "numeric_ext.c",
    "queue.c",
    "deps/pcg/entropy.c",
    "rng.c",
    "route.c",
    "solution.c",
    "string_ext.c",
    "timer.c",
    "tsp.c",
    "tspi.c",
    "util.c",
    "vrp.c",
    "vrptw.c"
]
_srcdir = "src/"
src = [_srcdir + s for s in _src]
src.append("py/pyvrp.pyx")

# add undef_macros = [ "NDEBUG" ] to undefine NDEBUG when compiling
ext = Extension(name="pyvrp", sources=src, undef_macros = [ "NDEBUG" ])
# ext = Extension(name="pyer", sources=src)
setup(ext_modules=cythonize(ext))
