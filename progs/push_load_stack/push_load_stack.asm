SECTION "Header", ROM0[$100]
  JP SimpleSprite
  DS $150 - @, 0

SimpleSprite:
  ld bc, $1234
  push bc
  call ClearMem
  pop bc ; ClearMem - addr
  

ClearMem:
  ld hl, sp+2

  ld a, [hli]
  ld c, a
  ld a, [hli]
  ld b, a

Done:
  jp Done
