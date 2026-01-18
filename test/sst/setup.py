import setuptools
from Cython.Build import cythonize


setuptools.setup(
    ext_modules=cythonize(["src/sst_json_tests.py"]),
    package_dir={"sst_json_tests": "src/"},
)
