cimport gfgb 

def main():
  gb_state = gfgb.gb_state_alloc()
  gfgb.gb_state_init(gb_state)
  gfgb.set_r8(gb_state, gfgb.R8_B, 10)
  print(gfgb.get_r8(gb_state, gfgb.R8_B))

if __name__ == "__main__":
  main()
