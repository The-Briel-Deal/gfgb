; This is just a simple test to make sure that the voluntary wave channel (ch3)
; works, it just plays two tones in a loop.
; Verified on real DMG-01 hardware.


INCLUDE "defines.inc"

; Args:
;   Period: 11 bit number (16 bit with most significant 5 bits masked off)
MACRO ch3_set_period_and_trigger
  DEF period EQU \1
  ld bc, period
  ; Lower 8
  ld a, c
  ldh [rNR33], a

  ; Upper 3
  ld a, b
  and a, 0b0000_0111
  set 7, a
  ldh [rNR34], a
  PURGE period
ENDM

SECTION "Intro", ROMX
Intro::
  ld a, 0b1_000_0_0_0_0 ; [on/off]_[unused]_[RO ch4 on]_[RO ch3 on]_[RO ch2 on]_[RO ch1 on]
  ldh [rNR52], a        ; Turn on sound.

  ld a, $ff
  ldh [rNR50], a  ; Full volume, both channels on.
  ldh [rNR51], a  ; All sounds to all terminals.
  ; Enable DAC
  ld a, 0b1000_0000
  ldh [rNR30], a
  ; Set length timer to 255
  ld a, 255
  ldh [rNR31], a

  ; Set output level to 100% volume 
  ld a, 0b0_01_00000
  ldh [rNR32], a

  ld de, square_wave ; @param  de: Pointer to beginning of block to copy
  ld hl, _AUD3WAVERAM 
  ld bc, AUD3WAVE_SIZE

  call Memcpy

.loop:
  ch3_set_period_and_trigger 512
  REPT 60
  call WaitVBlank
  ENDR
  ch3_set_period_and_trigger 1023
  REPT 60
  call WaitVBlank
  ENDR

  jp .loop

SECTION "WaveData", ROMX
square_wave::
REPT 4
  db 0b1111_1111, 0b1111_1111,
ENDR
REPT 4
  db 0b0000_0000, 0b0000_0000,
ENDR
  
