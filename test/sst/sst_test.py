from dataclasses import dataclass
import json
import pathlib
from types import ModuleType
import pytest

from typing import TYPE_CHECKING, Any, List, Optional, Union

if TYPE_CHECKING:
  import gfgb_py


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


def load_initial_state(
    gfgb: ModuleType, gb_state: gfgb_py.GB_State, state: StateSnapshot
):
  gb_state.set_r8(gfgb.R8.B, state.b)
  gb_state.set_r8(gfgb.R8.C, state.c)
  gb_state.set_r8(gfgb.R8.D, state.d)
  gb_state.set_r8(gfgb.R8.E, state.e)
  gb_state.set_r8(gfgb.R8.H, state.h)
  gb_state.set_r8(gfgb.R8.L, state.l)
  gb_state.set_r8(gfgb.R8.A, state.a)
  gb_state.set_r16(gfgb.R16.SP, state.sp)
  gb_state.set_pc(state.pc)


def assert_state_equals(
    gfgb: ModuleType, gb_state: gfgb_py.GB_State, state: StateSnapshot
):
  assert gb_state.get_r8(gfgb.R8.B) == state.b
  assert gb_state.get_r8(gfgb.R8.C) == state.c
  assert gb_state.get_r8(gfgb.R8.D) == state.d
  assert gb_state.get_r8(gfgb.R8.E) == state.e
  assert gb_state.get_r8(gfgb.R8.H) == state.h
  assert gb_state.get_r8(gfgb.R8.L) == state.l
  assert gb_state.get_r8(gfgb.R8.A) == state.a
  assert gb_state.get_r16(gfgb.R16.SP) == state.sp
  assert gb_state.get_pc() == state.pc


@pytest.mark.parametrize("test_file_path", test_files)
def test_single_step(test_file_path: pathlib.Path, gfgb_py_mod: ModuleType):
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

    gb_state = gfgb_py_mod.GB_State()

    load_initial_state(gfgb_py_mod, gb_state, sst_case.initial)
    # Just to make sure setting and getting line up.
    assert_state_equals(gfgb_py_mod, gb_state, sst_case.initial)
