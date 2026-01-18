from setuptools import setup
from Cython.Build import cythonize

setup(ext_modules=cythonize("sst_json_tests.py"))
