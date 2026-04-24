; TODO: Write about motivations and expected results of this test.


INCLUDE "defines.inc"

; Args:
;   Period: 11 bit number (16 bit with most significant 5 bits masked off)
MACRO ch1_set_period
  DEF period EQU \1
  ld bc, period
  ; Lower 8
  ld a, c
  ldh [rNR13], a

  ; Upper 3
  ld a, b
  and a, 0b0000_0111
  ldh [rNR14], a
  PURGE period
ENDM

SECTION "Intro", ROMX
Intro::
  ld a, 0b1_000_0_0_0_0 ; [on/off]_[unused]_[RO ch4 on]_[RO ch3 on]_[RO ch2 on]_[RO ch1 on]
  ldh [rNR52], a        ; Turn on sound.

  ld a, $ff
  ldh [rNR50], a  ; Full volume, both channels on.
  ldh [rNR51], a  ; All sounds to all terminals.
  ld a, 0b0000_0000
  ldh [rNR10], a
  ld a, 0b1000_0000
  ldh [rNR11], a
  ld a, 0b1111_1010
  ldh [rNR12], a

  ch1_set_period 2047

  ; Trigger Channel 1
  ld  hl, rNR14
  set 7, [hl]

  jr @
