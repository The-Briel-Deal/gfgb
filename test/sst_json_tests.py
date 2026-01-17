import ctypes

class GBState(ctypes.Structure):
  pass


def main():
  gfgb = ctypes.CDLL("./build/libgfgb.so")
  gfgb.gb_state_alloc.restype = ctypes.POINTER(GBState)
  gb_state = gfgb.gb_state_alloc()
  gfgb.gb_state_init(gb_state)


if __name__ == "__main__":
  main()
