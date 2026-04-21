; If an OAM entry has it's priority bit set then it should be displayed behind
; Window and BG pixels that have an index of 1, 2, or 3. However it is unclear
; to me at the time of writing what happens in this case.
;
; (Assume these are at the same display location)
; 1. Obj (Priority = true)
; 2. BG Tile (All pixels are palette index 1)
; 3. Win Tile (All pixels are palette index 0)
;
; Which tile would show up on top? Since the Win tile covers the BG with all
; index 0, does that mean that the obj gets shown? Or is the object hidden
; because it's covered by the BG tile?
;
; Thats the purpose of this test. I'de like to make sure my emulator has the
; same behavior as original hardware.


INCLUDE "defines.inc"

def TILEDATA_BLK0 equ $8000
def TILEDATA_BLK1 equ $8800
def TILEDATA_BLK2 equ $9000

SECTION "Tile Data", ROMX

black_tile: 
REPT 8
  db 0b1111_1111, 0b1111_1111,
ENDR

dark_grey_tile: 
REPT 8
  db 0b1111_1111, 0b0000_0000,
ENDR

light_grey_tile: 
REPT 8
  db 0b0000_0000, 0b1111_1111,
ENDR

white_tile: 
REPT 8
  db 0b0000_0000, 0b0000_0000
ENDR

; Args:
;   Src
;   Dst
MACRO set_tiledata
  ld de, \1                  ; Src
  ld hl, TILEDATA_BLK0 + ($10 * \2) ; Dst
  ld bc, $10                 ; Len
  call LCDMemcpy

ENDM

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
  set_tiledata black_tile, 0
  ; Make tile index 1 dark grey
  set_tiledata dark_grey_tile, 1
  ; Make tile index 2 light grey
  set_tiledata light_grey_tile, 2
  ; Make tile index 3 white
  set_tiledata white_tile, 3


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
