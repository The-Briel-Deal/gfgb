from dataclasses import dataclass
from re import sub
from typing import Any, List, Optional, Union
import json
import pathlib
import pytest

import gfgb


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
test_files = [path.name for path in sst_test_dir.iterdir()]
assert len(test_files) == 500


def load_initial_state(gb_state: gfgb.GB_State, state: StateSnapshot):
  gb_state.set_r8(gfgb.R8.B, state.b)
  gb_state.set_r8(gfgb.R8.C, state.c)
  gb_state.set_r8(gfgb.R8.D, state.d)
  gb_state.set_r8(gfgb.R8.E, state.e)
  gb_state.set_r8(gfgb.R8.H, state.h)
  gb_state.set_r8(gfgb.R8.L, state.l)
  gb_state.set_r8(gfgb.R8.A, state.a)
  gb_state.set_r16(gfgb.R16.SP, state.sp)
  gb_state.set_f(state.f)
  gb_state.set_pc(state.pc)
  assert state.ime == 0 or state.ime == 1
  gb_state.set_ime(state.ime != 0)
  # Load ram
  for entry in state.ram:
    addr = entry[0]
    val = entry[1]
    gb_state.write_mem8(addr, val)


def assert_state_equals(gb_state: gfgb.GB_State, state: StateSnapshot):
  assert gb_state.get_r8(gfgb.R8.B) == state.b
  assert gb_state.get_r8(gfgb.R8.C) == state.c
  assert gb_state.get_r8(gfgb.R8.D) == state.d
  assert gb_state.get_r8(gfgb.R8.E) == state.e
  assert gb_state.get_r8(gfgb.R8.H) == state.h
  assert gb_state.get_r8(gfgb.R8.L) == state.l
  assert gb_state.get_r8(gfgb.R8.A) == state.a
  assert gb_state.get_r16(gfgb.R16.SP) == state.sp
  assert gb_state.get_f() == state.f
  assert gb_state.get_pc() == state.pc
  assert gb_state.get_ime() == state.ime
  # Check ram
  for entry in state.ram:
    addr = entry[0]
    expect_val = entry[1]
    result_val = gb_state.read_mem8(addr)
    assert expect_val == result_val


@pytest.mark.parametrize(
    "test_file_name",
    test_files,
    ids=[sub("\\.json", "", file_name) for file_name in test_files],
)
def test_single_step(test_file_name: str):
  test_file_path = pathlib.Path(sst_test_dir / test_file_name)
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

    gb_state = gfgb.GB_State()
    gb_state.use_flat_mem(True)

    load_initial_state(gb_state, sst_case.initial)

    gb_state.fetch_and_exec()
    assert gb_state.get_err() == False

    assert_state_equals(gb_state, sst_case.final)
