INCLUDE "hardware.inc"

SECTION "Header", ROM0[$100]
  JP SimpleSprite
  DS $150 - @, 0

SimpleSprite:
  ; Shut down audio circuitry
  ld a, 0
  ld [rNR52], a
  call WaitForVBlank

  call LCDOff

  ld d, 16
  ld hl, $9010
  ld bc, DoggoSprite
  call CopySprite

  call LCDOn

  ; During the first (blank) frame, initialize display registers
  ld a, %11100100
  ld [rBGP], a
  
  call Done


WaitForVBlank:
  ld a, [rLY]
  cp 144
  jp c, WaitForVBlank
  ret


CopySprite:
  ld A, [BC]
  ld [HLI], A
  inc BC
  dec D
  jr NZ, CopySprite
  ret
  

LCDOff:
  ld a, 0
  ld [rLCDC], a
  ret


LCDOn:
  ld a, LCDCF_ON | LCDCF_BGON
  ld [rLCDC], a
  ret


Done:
  jp Done


DoggoSprite:
  INCBIN "doggo.bin"
