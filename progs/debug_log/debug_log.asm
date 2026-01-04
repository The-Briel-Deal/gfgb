SECTION "Header", ROM0[$100]
  JP DebugLog
  DS $150 - @, 0

DebugLog:
  ld d,d
  jr .end
  dw $6464
  dw $0001
  dw Msg
  dw 0
  .end:

Done:
  jp Done

Msg: 
  db "Test Debug Message"
