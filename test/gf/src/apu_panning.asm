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

; Args:
;   Period: 11 bit number (16 bit with most significant 5 bits masked off)
MACRO ch2_set_period_and_trigger
  DEF period EQU \1
  ld bc, period
  ; Lower 8
  ld a, c
  ldh [rNR23], a

  ; Upper 3
  ld a, b
  and a, 0b0000_0111
  set 7, a
  ldh [rNR24], a
  PURGE period
ENDM

SECTION "Intro", ROMX
Intro::
  ld a, 0b1_000_0_0_0_0 ; [on/off]_[unused]_[RO ch4 on]_[RO ch3 on]_[RO ch2 on]_[RO ch1 on]
  ldh [rNR52], a        ; Turn on sound.

  ld a, $ff
  ldh [rNR50], a  ; Full volume, both channels on.
  ld a, 0b1000_0000
  ldh [rNR11], a

.loop:
  ;; Channel 1
  ; Turn on Channel 1 (by setting volume)
  ld a, 0b1111_1010
  ldh [rNR12], a

  ld a, 0b0000_0001
  ldh [rNR51], a  ; Right Channel Only
  ch1_set_period_and_trigger 512
  REPT 60
  call WaitVBlank
  ENDR
  ld a, 0b0001_0000
  ldh [rNR51], a  ; Left Channel Only
  ch1_set_period_and_trigger 1023
  REPT 60
  call WaitVBlank
  ENDR

  ; Turn off Channel 1
  ld a, 0b0000_0000
  ldh [rNR12], a

  ;; Channel 2
  ; Turn on Channel 2 (by setting volume)
  ld a, 0b1111_1010
  ldh [rNR22], a
  ld a, 0b0000_0010
  ldh [rNR51], a  ; Right Channel Only
  ch2_set_period_and_trigger 512
  REPT 60
  call WaitVBlank
  ENDR
  ld a, 0b0010_0000
  ldh [rNR51], a  ; Left Channel Only
  ch2_set_period_and_trigger 1023
  REPT 60
  call WaitVBlank
  ENDR

  ; Turn off Channel 2
  ld a, 0b0000_0000
  ldh [rNR22], a

  jp .loop
