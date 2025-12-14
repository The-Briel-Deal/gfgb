INCLUDE "hardware.inc"

SECTION "Header", ROM0[$100]
  JP SimpleSprite
  DS $150 - @, 0

SimpleSprite:
	; Shut down audio circuitry
	ld a, 0
	ld [rNR52], a
  .wait_for_vblank:
	ld a, [rLY]
	cp 144
	jp c, .wait_for_vblank

	; Turn the LCD off
	ld a, 0
	ld [rLCDC], a

  LD D, 16
  LD HL, $9000
  LD BC, DoggoSprite
  .loop:
  LD A, [BC]
  LD [HLI], A
  INC BC
  DEC D
  JR NZ, .loop
  ; Turn the LCD on
  ld a, LCDCF_ON | LCDCF_BGON
  ld [rLCDC], a
  ; During the first (blank) frame, initialize display registers
  ld a, %11100100
  ld [rBGP], a
  .done:
  jp .done

DoggoSprite:
  INCBIN "doggo.bin"
