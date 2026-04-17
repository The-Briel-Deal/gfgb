INCLUDE "defines.inc"


SECTION "Intro", ROMX

Intro::
  ld hl, rLCDC
  set 4, [hl]

  ld hl, $8000
  ld bc, $10
  ld a, $FF
  call LCDMemset

  jr @
