from distutils.core import setup, Extension
from Cython.Build import cythonize

# list of all source files for building the lib
_src = ["solver.c", "util.c", "rng.c"]
_srcdir = "src/"
src = [_srcdir + s for s in _src]
src.append("py/pyer.pyx")

ext = Extension(name="pyer", sources=src)
setup(ext_modules=cythonize(ext))
