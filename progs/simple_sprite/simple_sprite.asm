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

  ld a, 16
  push af

  ld hl, $9010


  ld bc, DoggoSprite
  
  call CopySprite

  pop af

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
  push hl
  ld hl, sp + 5
  ld d, [hl]
  pop hl
  .loop:
  ld a, [bc]
  ld [hli], a
  inc bc
  dec d
  jr nz, .loop
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
