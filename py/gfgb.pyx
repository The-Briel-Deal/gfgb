import enum
import cython

from cython.cimports import gfgb  # type: ignore


class R8(enum.IntEnum):
  B = 0
  C = 1
  D = 2
  E = 3
  H = 4
  L = 5
  HL_DREF = 6
  A = 7


@cython.cclass
class GB_State:
  _gb_state: cython.pointer[gfgb.gb_state]

  def __cinit__(self):
    self._gb_state = gfgb.gb_state_alloc()
    gfgb.gb_state_init(self._gb_state)

  def set_r8(self, reg: R8, val: cython.uint):
    gfgb.set_r8(self._gb_state, reg, val)

  def get_r8(self, reg: R8) -> cython.uint:
    return gfgb.get_r8(self._gb_state, reg)
