;
; sp.s -- one SmartPort call, for the FujiNet build (fuji.c).
;
; A SmartPort card's firmware is entered at $Cn00 + ($CnFF) + 3 with the
; command byte and the address of a parameter list inline after the JSR;
; it returns past them with the result in A (0 for success) and the number
; of bytes moved in X (low) and Y (high).
;
; cc65's startup leaves language-card RAM readable at $D000-$FFFF while the
; program runs (blit.s). Card firmware may call the monitor or read the
; ROM's vectors, so the ROM is switched in for the call and the RAM back
; out after it, as scr_done() does.

        .export         _sp_set_entry
        .export         _sp_call
        .export         _sp_parms
        .export         _sp_count
        .export         _sp_raw_a

LCRAM           = $C080         ; read LC RAM bank 2, no write
LCROM           = $C082         ; read ROM, no write

        .bss

; The parameter list. fuji.c fills it: count, unit, buffer, then the
; command's own fields (a status or control code, or a byte count and a
; three-byte address).
_sp_parms:      .res    9
_sp_count:      .res    2
_sp_raw_a:      .res    1       ; A as the card returned it

        .code

; void __fastcall__ sp_set_entry(unsigned int entry)
_sp_set_entry:
        sta     entry
        stx     entry+1
        rts

; unsigned char __fastcall__ sp_call(unsigned char command)
; 0, or the SmartPort error code. sp_count holds the bytes moved.
_sp_call:
        sta     command
        bit     LCROM
        jsr     $FFFF
entry   = *-2
command:
        .byte   0
        .addr   _sp_parms
        php
        bit     LCRAM           ; leaves A alone
        sta     _sp_raw_a
        stx     _sp_count
        sty     _sp_count+1
        plp
        bcs     @err            ; the error is the carry; A is its code
        lda     #0
@err:   ldx     #0
        rts
