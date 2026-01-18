import sys
import pathlib
import pytest
# This is a janky way to make sure that my cython module is in path. I'll figure out a better solution later.
sys.path.append("build")

import gfgb_py


sst_test_dir = pathlib.Path(__file__).parent / "test_json/"
assert sst_test_dir.is_dir()
test_files = list(sst_test_dir.iterdir())
assert len(test_files) == 500


@pytest.mark.parametrize("test_file", test_files)
def test_single_step(test_file: pathlib.Path):
  assert test_file.is_file()
