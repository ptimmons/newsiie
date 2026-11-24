;
; netcall.s -- the app's three entries to the network, passed on to the
; loaded driver's table (netdrv.h) with the registers as they came, so a
; __fastcall__ argument in A/X goes through untouched. With no driver,
; netdrv.c's stand-ins answer.
;
        .export         _net_check, _source_stream, _source_net_error
        .export         _net_loaded
        .import         __DRVCOGS_START__
        .import         _net_none_check, _net_none_error

SLOT    = __DRVCOGS_START__     ; both drivers' areas start here
FIND    = SLOT + 3
CHECK   = SLOT + 6
STREAM  = SLOT + 9
ERROR   = SLOT + 12

SRC_E_NONET     = $F7           ; source.h

        .export         _net_find
_net_find       = FIND          ; netdrv.c calls it after a load

        .bss
_net_loaded:    .res    1       ; nonzero once a driver found its device

        .code

; void net_check(void)
_net_check:
        bit     _net_loaded
        bmi     :+
        jmp     _net_none_check
:       jmp     CHECK

; uint8_t __fastcall__ source_stream(const char *url)
_source_stream:
        bit     _net_loaded
        bmi     :+
        lda     #SRC_E_NONET
        ldx     #0
        rts
:       jmp     STREAM

; const char *__fastcall__ source_net_error(uint8_t rc)
_source_net_error:
        bit     _net_loaded
        bmi     :+
        jmp     _net_none_error
:       jmp     ERROR
