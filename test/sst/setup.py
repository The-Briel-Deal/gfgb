import os
import pathlib
from setuptools import setup
from Cython.Build import cythonize


root_dir = pathlib.Path(__file__).parent
src_dir = root_dir / "src/"
setup(
    ext_modules=cythonize(str((src_dir / "sst_json_tests.py").resolve())),
)
