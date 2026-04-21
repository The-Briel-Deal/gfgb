INCLUDE "defines.inc"


SECTION "Intro", ROMX

Intro::
  ; Set BG Tile Data block to $8000 and tile map to $9C00
  ld hl, rLCDC
  set 4, [hl]
  set 3, [hl]

  ld hl, rBGP
  ld [hl], 0b00011011

  ld hl, $8000
  ld bc, $10
  ld a, $FF
  call LCDMemset

  jr @
