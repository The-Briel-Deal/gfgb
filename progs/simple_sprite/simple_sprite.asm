SECTION "Header", ROM0[$100]
  jp SimpleSprite
  ds $150 - @, 0
SimpleSprite:
  ld HL, $C000
  ld a, $3D
  ld [HL+], a
  ld a, $4D
  ld [HL+], a
