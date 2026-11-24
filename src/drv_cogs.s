;
; drv_cogs.s -- NEWSIIE.COGS begins with this table (netdrv.h), linked into
; the COGS* segments with the rest of the CoGS driver: cogs.c, slot.c,
; cogs_io.s, net.c and src_cogs.c (Makefile).
;
        .import         __ONCE_RUN__
        .import         _cogs_find, _cogs_net_check, _cogs_stream, _cogs_net_error

        .segment        "COGSHDR"

        .byte   $D5                     ; NETDRV_MAGIC
        .addr   __ONCE_RUN__            ; the stamp
        jmp     _cogs_find              ; the slot, or 0: nonzero is found
        jmp     _cogs_net_check
        jmp     _cogs_stream
        jmp     _cogs_net_error
