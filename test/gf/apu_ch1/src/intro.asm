; This is just a simple first test to make sure that the pulse width channels
; work, it just plays two tones in a loop.
; Verified on real DMG-01 hardware.


INCLUDE "defines.inc"

; Args:
;   Period: 11 bit number (16 bit with most significant 5 bits masked off)
MACRO ch1_set_period_and_trigger
  DEF period EQU \1
  ld bc, period
  ; Lower 8
  ld a, c
  ldh [rNR13], a

  ; Upper 3
  ld a, b
  and a, 0b0000_0111
  set 7, a
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
  ld a, 0b1000_0000
  ldh [rNR11], a
  ld a, 0b1111_1010
  ldh [rNR12], a

.loop:
  ch1_set_period_and_trigger 512
  REPT 60
  call WaitVBlank
  ENDR
  ch1_set_period_and_trigger 1023
  REPT 60
  call WaitVBlank
  ENDR

  jp .loop
