; TODO: Write about motivations and expected results of this test.


INCLUDE "defines.inc"

SECTION "Intro", ROMX
Intro::
  ld a, %10001111
  ldh [rAUDENA], a            ; Turn on sound.
  ld a, $ff
  ldh [rAUDVOL], a            ; Full volume, both channels on.
  ldh [rAUDTERM], a           ; All sounds to all terminals.
  ld a, 0b1000_0000
  ldh [rNR11], a
  ld a, 0b1111_1010
  ldh [rNR12], a
  ld a, 0b1111_1010
  ldh [rNR13], a
Trigger:
  ld a, $ff
  ldh [rNR14], a
  jr Trigger
