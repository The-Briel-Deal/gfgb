INCLUDE "defines.inc"

def TILEDATA_BLK0 equ $8000
def TILEDATA_BLK1 equ $8800
def TILEDATA_BLK2 equ $9000

SECTION "Tile Data", ROMX

black_tile: 
  db 0b1111_1111, 0b1111_1111,
  db 0b1111_1111, 0b1111_1111,
  db 0b1111_1111, 0b1111_1111,
  db 0b1111_1111, 0b1111_1111,
  db 0b1111_1111, 0b1111_1111,
  db 0b1111_1111, 0b1111_1111,
  db 0b1111_1111, 0b1111_1111,
  db 0b1111_1111, 0b1111_1111,

dark_grey_tile: 
  db 0b1111_1111, 0b0000_0000,
  db 0b1111_1111, 0b0000_0000,
  db 0b1111_1111, 0b0000_0000,
  db 0b1111_1111, 0b0000_0000,
  db 0b1111_1111, 0b0000_0000,
  db 0b1111_1111, 0b0000_0000,
  db 0b1111_1111, 0b0000_0000,
  db 0b1111_1111, 0b0000_0000,


light_grey_tile: 
  db 0b0000_0000, 0b1111_1111,
  db 0b0000_0000, 0b1111_1111,
  db 0b0000_0000, 0b1111_1111,
  db 0b0000_0000, 0b1111_1111,
  db 0b0000_0000, 0b1111_1111,
  db 0b0000_0000, 0b1111_1111,
  db 0b0000_0000, 0b1111_1111,
  db 0b0000_0000, 0b1111_1111,

white_tile: 
  db 0b0000_0000, 0b0000_0000,
  db 0b0000_0000, 0b0000_0000,
  db 0b0000_0000, 0b0000_0000,
  db 0b0000_0000, 0b0000_0000,
  db 0b0000_0000, 0b0000_0000,
  db 0b0000_0000, 0b0000_0000,
  db 0b0000_0000, 0b0000_0000,
  db 0b0000_0000, 0b0000_0000,

SECTION "Intro", ROMX


Intro::
  ; Set BG Tile Data block to $8000 and tile map to $9C00
  ld hl, hLCDC
  set 4, [hl]
  set 3, [hl]

  ld hl, hBGP
  ld [hl], 0b11_10_01_00

  ; Zero Tilemap
  ld hl, TILEMAP1 ; Start
  ld bc, $0400    ; Len
  ld a, 0         ; Fill Byte
  call LCDMemset

  ; Zero Tiledata
  ld hl, TILEDATA_BLK0 ; Start
  ld bc, $10           ; Len
  ld a, $00            ; Fill Byte
  call LCDMemset

  ; Make tile index 0 black
  ld de, black_tile          ; Src
  ld hl, TILEDATA_BLK0 + $00 ; Dst
  ld bc, $10                 ; Len
  call LCDMemcpy

  ; Make tile index 1 dark grey
  ld de, dark_grey_tile      ; Src
  ld hl, TILEDATA_BLK0 + $10 ; Dst
  ld bc, $10                 ; Len
  call LCDMemcpy

  ; Make tile index 2 light grey
  ld de, light_grey_tile     ; Src
  ld hl, TILEDATA_BLK0 + $20 ; Dst
  ld bc, $10                 ; Len
  call LCDMemcpy

  ; Make tile index 3 white
  ld de, white_tile     ; Src
  ld hl, TILEDATA_BLK0 + $30 ; Dst
  ld bc, $10                 ; Len
  call LCDMemcpy

  ld hl, TILEMAP1 + 0 ; Start
  ld bc, 1            ; Len
  ld a, 0             ; Fill Byte
  call LCDMemset

  ld hl, TILEMAP1 + 1 ; Start
  ld bc, 1            ; Len
  ld a, 1             ; Fill Byte
  call LCDMemset

  ld hl, TILEMAP1 + 2 ; Start
  ld bc, 1            ; Len
  ld a, 2             ; Fill Byte
  call LCDMemset

  ld hl, TILEMAP1 + 3 ; Start
  ld bc, 1            ; Len
  ld a, 3             ; Fill Byte
  call LCDMemset

  jr @
