SECTION "Header", ROM0[$100]
  JP SimpleSprite
  DS $150 - @, 0

SimpleSprite:
  LD A, $1B
  LD [$FF47], A

  LD D, 16
  LD HL, $8000
  LD BC, DoggoSprite
  .loop:
  LD A, [BC]
  LD [HLI], A
  INC BC
  DEC D
  JR NZ, .loop
  .loop_end:
  .forever_loop:
  NOP
  JP .forever_loop

DoggoSprite:
  DB 0x00, 0x00, 0x00, 0x00, 0x30, 0x50, 0xe9, 0x89, 0x3e, 0x02, 0x24, 0x18, 0x24, 0x24, 0x00, 0x00

