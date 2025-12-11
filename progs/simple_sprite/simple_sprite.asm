SECTION "Header", ROM0[$100]
  jp TestFunc
  ds $150 - @, 0
SECTION "My Code"
TestFunc:
  ld a, $03
