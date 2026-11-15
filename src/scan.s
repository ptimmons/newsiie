;
; scan.s -- the parser's inner loop (65C02, cc65 calling convention).
;
; unsigned int __fastcall__ scan(const unsigned char *p, unsigned int n,
;                                unsigned char mask)
;
; Offset of the first byte of p[0..n) whose scan_class bits meet mask, or
; n. Whole pages run in an 8-bit Y loop: lda/tax/lda/and/bne/iny/bne is
; 18 cycles a byte when nothing matches. scan.c is the same in C.

        .export         _scan
        .import         _scan_class
        .import         popax
        .importzp       ptr1, ptr2, ptr3, tmp1, tmp2

mask    = tmp1
lim     = tmp2
rem     = ptr2                  ; bytes not yet scanned
start   = ptr3                  ; p, to turn the end pointer into an offset

        .code

_scan:
        sta     mask
        jsr     popax           ; n
        sta     rem
        stx     rem+1
        jsr     popax           ; p
        sta     ptr1
        stx     ptr1+1
        sta     start
        stx     start+1

@page:  lda     rem+1
        bne     @full           ; 256 or more left: a whole page
        ldy     rem
        beq     @done           ; nothing left: ptr1 is the end
        sty     lim
        ldy     #0
@part:  lda     (ptr1),y
        tax
        lda     _scan_class,x
        and     mask
        bne     @at
        iny
        cpy     lim
        bne     @part
        bra     @at             ; none: Y = lim, the end

@full:  ldy     #0
@loop:  lda     (ptr1),y
        tax
        lda     _scan_class,x
        and     mask
        bne     @at
        iny
        bne     @loop
        inc     ptr1+1          ; next page
        dec     rem+1
        bra     @page

@at:    tya                     ; ptr1 += Y
        clc
        adc     ptr1
        sta     ptr1
        bcc     @done
        inc     ptr1+1
@done:  lda     ptr1            ; return ptr1 - start
        sec
        sbc     start
        pha
        lda     ptr1+1
        sbc     start+1
        tax
        pla
        rts
