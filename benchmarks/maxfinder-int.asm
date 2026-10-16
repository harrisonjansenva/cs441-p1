#maxfinder-int


main:
    LD   r2, 0x00000006
    LD   r3, 0x00000020
    BEQ  r0, r2,           done
    LD   r4, DATA(r3)
    DEC  r2
    BEQ  r0, r2,           done


loop:
    ADD  r1, r3                           # Add 1 to the pointer
    LD   r5, DATA(r3)
    BLT  r5, r4,           incr
    MOV  r5, r4


incr:        
    DEC  r2,
    BEQ  r0, r2,           done
    JUMP loop


done:
    ST   r4, 0x0000030
    HALT