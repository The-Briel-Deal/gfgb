import cython

from cython.cimports import gfgb  # type: ignore



@cython.cclass
class GB_State:
  _gb_state: cython.pointer[gfgb.gb_state]
  def __cinit__(self):
    self._gb_state = gfgb.gb_state_alloc()
    gfgb.gb_state_init(self._gb_state)

  def set_r8(self, reg, val):
    gfgb.set_r8(self._gb_state, reg, val)

  def get_r8(self, reg):
    return gfgb.get_r8(self._gb_state, reg)
