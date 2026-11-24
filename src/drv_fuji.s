;
; drv_fuji.s -- NEWSIIE.FUJI begins with this table (netdrv.h), linked into
; the FUJI* segments with the rest of the FujiNet driver: fuji.c, sp.s,
; net_fuji.c and src_fuji.c (Makefile).
;
        .import         __ONCE_RUN__
        .import         _fuji_find, _fuji_net_check, _fuji_stream, _fuji_net_error

        .segment        "FUJIHDR"

        .byte   $D5                     ; NETDRV_MAGIC
        .addr   __ONCE_RUN__            ; the stamp
        jmp     _fuji_find
        jmp     _fuji_net_check
        jmp     _fuji_stream
        jmp     _fuji_net_error
