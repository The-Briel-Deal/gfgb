; TODO: Write about motivations and expected results of this test.


INCLUDE "defines.inc"

SECTION "Intro", ROMX
Intro::
  ld a, 0b1_000_0_0_0_0 ; [on/off]_[unused]_[RO ch4 on]_[RO ch3 on]_[RO ch2 on]_[RO ch1 on]
  ldh [rNR52], a        ; Turn on sound.

  ld a, $ff
  ldh [rNR50], a  ; Full volume, both channels on.
  ldh [rNR51], a  ; All sounds to all terminals.
  ld a, 0b1000_0000
  ldh [rNR11], a
  ld a, 0b1111_1010
  ldh [rNR12], a

  ld a, 255 ; Period lower 8
  ldh [rNR13], a
  ld a, 0b1_0_000_011 ; [trigger]_[len enable]_[unused]_[period upper]
  ldh [rNR14], a
  jr @
