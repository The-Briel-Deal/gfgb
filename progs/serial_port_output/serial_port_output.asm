SECTION "Header", ROM0[$100]
  JP DebugLog
  DS $150 - @, 0

DebugLog:
  ld a, 'c'
  ld [$FF01], a
  ld a, 'b'
  ld [$FF01], a

Done:
  jp Done

Msg: 
  db "Test Debug Message"
