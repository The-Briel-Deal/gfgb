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

  ; ClearMem - addr
  ld bc, _SCRN0 
  push bc
  ; ClearMem - fill byte (f is just padding to keep stack 2 byte aligned)
  ld a, $01
  push af
  ; ClearMem - len
  ld bc, 32 * 32
  push bc

  call ClearMem
  pop bc ; ClearMem - addr
  pop af ; ClearMem - fill byte
  pop bc ; ClearMem - len

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


/* d: the number of bytes to copy, lowest arg stored on stack.
 * TODO: Store the other arguments on the stack (destination and source addr)
 */
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
  

/* args: 
 *   addr: The address to start filling at. (goes into DE then HL at the end)
 *   fill byte: The byte to fill the memory with. (goes into A)
 *   len: The number of bytes to fill. (goes into BC)
 */
ClearMem:
  ld hl, sp+2

  ; len -> BC
  ld a, [hli]
  ld c, a
  ld a, [hli]
  ld b, a

  ; fill byte -> A
  ld a, [hli] ; padding
  ld a, [hli]
  push af

  ; addr -> DE (goes into hl after everything is loaded)
  ld a, [hli]
  ld e, a
  ld a, [hli]
  ld d, a

  pop af

  ld h, d
  ld l, e
  ld d, a

  .loop
  ld a, d
  ld [hli], a
  dec bc

  ld a, 0
  or a, b
  or a, c
  jr nz, .loop

  ret

LCDOff:
  ld a, 0
  ld [rLCDC], a
  ret


LCDOn:
  ld a, LCDCF_ON | LCDCF_BGON | LCDCF_BG9800 | LCDCF_BG8800
  ld [rLCDC], a
  ret


Done:
  jp Done


DoggoSprite:
  INCBIN "doggo.bin"
