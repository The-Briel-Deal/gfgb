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

; Args:
;   1: Src
;   2: Dst
MACRO set_tiledata0
  ld de, \1                         ; Src
  ld hl, TILEDATA_BLK0 + ($10 * \2) ; Dst
  ld bc, $10                        ; Len
  call LCDMemcpy
ENDM

; Args:
;   1: TilemapIndex
;   2: TiledataIndex
MACRO set_tilemap1
  ld hl, TILEMAP1 + \1 ; Start
  ld bc, 1             ; Len
  ld a, \2             ; Fill Byte
  call LCDMemset
ENDM


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

white_black_tile: 
REPT 8
  db 0b1111_1111, 0b0000_1111
ENDR

SECTION "OAM", ROMX, ALIGN[8]
oam_data:
  ; OAM 0
  db 16          ; Y Position
  db 8           ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b0000_0000 ; Flags, no prio (bit 7)
  ; OAM 1
  db 16          ; Y Position
  db 16          ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b0000_0000 ; Flags, no prio (bit 7)
  ; OAM 2
  db 16          ; Y Position
  db 24           ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b0000_0000 ; Flags, no prio (bit 7)
  ; OAM 3
  db 16          ; Y Position
  db 32          ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b0000_0000 ; Flags, no prio (bit 7)
  ; OAM 4
  db 24          ; Y Position
  db 8           ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b1000_0000 ; Flags, has prio (bit 7)
  ; OAM 5
  db 24          ; Y Position
  db 16          ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b1000_0000 ; Flags, has prio (bit 7)
  ; OAM 6
  db 24          ; Y Position
  db 24           ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b1000_0000 ; Flags, has prio (bit 7)
  ; OAM 7
  db 24          ; Y Position
  db 32          ; X Position
  db 4           ; Tile Index (White and Black tile)
  db 0b1000_0000 ; Flags, has prio (bit 7)
  ; Just pad the rest of OAM with 0
  REPT 32
   db $00, $00, $00, $00
  ENDR

SECTION "Intro", ROMX


Intro::
  ; Set BG Tile Data block to $8000 and tile map to $9C00
  ld hl, hLCDC
  set 4, [hl]
  set 3, [hl]
  set 1, [hl]

  ld bc, oam_data
  ld a, b

	ldh [hOAMHigh], a

  ld hl, hBGP
  ld [hl], 0b11_10_01_00
  ld hl, hOBP0
  ld [hl], 0b11_10_00_00 ; Lower 2 bits are ignored since they are transparent
  ld hl, hOBP1
  ld [hl], 0b11_10_00_00 ; Lower 2 bits are ignored since they are transparent

  ; Zero Tilemap
  ld hl, TILEMAP1 ; Start
  ld bc, $0400    ; Len
  ld a, 1         ; Fill Byte
  call LCDMemset
  ; Zero Tiledata
  ld hl, TILEDATA_BLK0 ; Start
  ld bc, $10           ; Len
  ld a, $00            ; Fill Byte
  call LCDMemset


  ; Make tile index 0 black
  set_tiledata0 black_tile, 0
  ; Make tile index 1 dark grey
  set_tiledata0 dark_grey_tile, 1
  ; Make tile index 2 light grey
  set_tiledata0 light_grey_tile, 2
  ; Make tile index 3 white
  set_tiledata0 white_tile, 3
  ; Make tile index 4 half black half white
  set_tiledata0 white_black_tile, 4


  ; This line has overlapping no-prio objs
  set_tilemap1 0, 0
  set_tilemap1 1, 1
  set_tilemap1 2, 2
  set_tilemap1 3, 3

  ; This line has overlapping prio objs
  set_tilemap1 32 + 0, 0
  set_tilemap1 32 + 1, 1
  set_tilemap1 32 + 2, 2
  set_tilemap1 32 + 3, 3

  jr @
