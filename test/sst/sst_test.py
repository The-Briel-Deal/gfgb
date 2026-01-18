from dataclasses import dataclass
import json
import pathlib
import pytest
import sys

from typing import Any, List, Optional, Union

# This is a janky way to make sure that my cython module is in path. I'll figure out a better solution later.
sys.path.append("build")

from gfgb_py import GB_State, R8


@dataclass
class StateSnapshot:
  a: int
  b: int
  c: int
  d: int
  e: int
  f: int
  h: int
  l: int
  pc: int
  sp: int
  ime: int
  ram: List[List[int]]
  ei: Optional[int] = None
  ie: Optional[int] = None


@dataclass
class SSTCase:
  name: str
  initial: StateSnapshot
  final: StateSnapshot
  cycles: List[List[Union[int, str]]]


sst_test_dir = pathlib.Path(__file__).parent / "test_json/"
assert sst_test_dir.is_dir()
test_files = list(sst_test_dir.iterdir())
assert len(test_files) == 500


def load_initial_state(gb_state: GB_State, state: StateSnapshot):
  gb_state.set_r8(R8.A, state.a)


@pytest.mark.parametrize("test_file_path", test_files)
def test_single_step(test_file_path: pathlib.Path):
  assert test_file_path.is_file()
  test_file = test_file_path.open()
  test_data: list[dict[str, Any]] = json.load(test_file)
  for case in test_data:
    sst_case = SSTCase(
        name=case["name"],
        initial=StateSnapshot(**case["initial"]),
        final=StateSnapshot(**case["final"]),
        cycles=case["cycles"],
    )

    gb_state = GB_State()

    load_initial_state(gb_state, sst_case.initial)
