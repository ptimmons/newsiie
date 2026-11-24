/*
 * The FujiNet over SmartPort. See fuji.h for the protocol.
 */
#include <string.h>
#include "fuji.h"
#include "fmt.h"

#define SP_STATUS       0x00
#define SP_CONTROL      0x04
#define SP_READ         0x08

#define ST_DCB          0x00    /* unit 0: the number of units */
#define ST_DIB          0x03

#define NET_OPEN        'O'
#define NET_CLOSE       'C'
#define NET_STATUS      'S'
#define FUJI_WIFI       0xFA
#define FUJI_CONFIG     0xE8

#define MODE_READ       4
#define TRANS_NONE      0

#define CONFIG_LEN      140
#define CONFIG_IP       97
#define CONFIG_VERSION  125

#define URL_MAX         127

uint8_t fuji_slot;
uint8_t fuji_net;
uint8_t fuji_dev;
uint8_t fuji_err;

/* Status replies and control lists. The longest are the adapter's
 * configuration (140 bytes) and an open: 2 + 2 + "N:" + 127 + 1. */
static uint8_t buf[CONFIG_LEN + 4];

static uint8_t call(uint8_t cmd, uint8_t unit, uint8_t *data, uint8_t code)
{
    sp_parms[0] = 3;
    sp_parms[1] = unit;
    sp_parms[2] = (uint8_t)(uint16_t)data;
    sp_parms[3] = (uint8_t)((uint16_t)data >> 8);
    sp_parms[4] = code;
    fuji_err = sp_call(cmd);
    return fuji_err ? FN_E_BUS : 0;
}

/* A SmartPort card: $Cn01 $20, $Cn03 $00, $Cn05 $03, $Cn07 $00. A Disk II
 * has $3C at $Cn07, so this never picks one; reading slot ROM is safe. */
static uint8_t is_smartport(uint8_t s)
{
    const volatile uint8_t *rom = (const volatile uint8_t *)(0xC000 | ((uint16_t)s << 8));

    if (rom[1] != 0x20 || rom[3] != 0x00 || rom[5] != 0x03 || rom[7] != 0x00) {
        return 0;
    }
    sp_set_entry((uint16_t)rom + rom[0xFF] + 3);
    return 1;
}

static uint8_t named(const char *name, uint8_t len)
{
    return (uint8_t)(buf[4] == len && !memcmp(buf + 5, name, len));
}

uint8_t fuji_find(void)
{
    uint8_t s, u, n;

    if (fuji_slot) {
        return 1;
    }
    for (s = 7; s >= 1; --s) {
        if (!is_smartport(s) || call(SP_STATUS, 0, buf, ST_DCB)) {
            continue;
        }
        n = buf[0] > 32 ? 32 : buf[0];
        fuji_net = fuji_dev = 0;
        for (u = 1; u <= n; ++u) {
            if (call(SP_STATUS, u, buf, ST_DIB)) {
                continue;
            }
            if (named("NETWORK", 7) && !fuji_net) {
                fuji_net = u;
            } else if (named("THE_FUJI", 8)) {
                fuji_dev = u;
            }
        }
        if (fuji_net) {
            fuji_slot = s;
            return 1;
        }
    }
    return 0;
}

uint8_t fuji_open(const char *url)
{
    uint8_t n = (uint8_t)strlen(url);

    if (n > URL_MAX) {
        n = URL_MAX;
    }
    /* length, mode, translation, "N:", the URL, a zero */
    buf[0] = (uint8_t)(2 + 2 + n + 1);
    buf[1] = 0;
    buf[2] = MODE_READ;
    buf[3] = TRANS_NONE;
    buf[4] = 'N';
    buf[5] = ':';
    memcpy(buf + 6, url, n);
    buf[6 + n] = 0;
    return call(SP_CONTROL, fuji_net, buf, NET_OPEN);
}

uint8_t fuji_status(fuji_status_t *s)
{
    uint8_t rc = call(SP_STATUS, fuji_net, buf, NET_STATUS);

    if (!rc) {
        s->avail = (uint16_t)(buf[0] | (buf[1] << 8));
        s->conn = buf[2];
        s->err = buf[3];
    }
    return rc;
}

/* A READ's parameter count. The SmartPort standard says 4 (unit, buffer,
 * count, address), but the Yellowstone card refuses that for the network
 * device with $04 (bad parameter count), and fujinet-lib sends 5, with the
 * network unit (1) in the address's first byte. So try 4 once, and if it is
 * refused, use 5 from then on. */
#define SP_BADPCNT      0x04
uint8_t fuji_read_pcount = 4;

uint8_t fuji_read(uint8_t *dst, uint16_t n)
{
    sp_parms[1] = fuji_net;
    sp_parms[2] = (uint8_t)(uint16_t)dst;
    sp_parms[3] = (uint8_t)((uint16_t)dst >> 8);
    sp_parms[4] = (uint8_t)n;
    sp_parms[5] = (uint8_t)(n >> 8);
    for (;;) {
        sp_parms[0] = fuji_read_pcount;
        sp_parms[6] = (uint8_t)(fuji_read_pcount == 5);
        sp_parms[7] = sp_parms[8] = 0;
        sp_count = 0;
        fuji_err = sp_call(SP_READ);
        if (fuji_err != SP_BADPCNT || fuji_read_pcount == 5) {
            break;
        }
        fuji_read_pcount = 5;
    }
    return fuji_err ? FN_E_BUS : 0;
}

uint8_t fuji_close(void)
{
    buf[0] = buf[1] = 0;
    return call(SP_CONTROL, fuji_net, buf, NET_CLOSE);
}

uint8_t fuji_wifi(void)
{
    /* No THE_FUJI unit (an old firmware): assume yes, and let the open say. */
    if (!fuji_dev) {
        return 1;
    }
    return (uint8_t)(!call(SP_STATUS, fuji_dev, buf, FUJI_WIFI) && buf[0] == 3);
}

uint8_t fuji_config(uint8_t ip[4], char version[15])
{
    uint8_t rc;

    if (!fuji_dev) {
        return FN_E_NODEV;
    }
    rc = call(SP_STATUS, fuji_dev, buf, FUJI_CONFIG);
    if (rc) {
        return rc;
    }
    memcpy(ip, buf + CONFIG_IP, 4);
    memcpy(version, buf + CONFIG_VERSION, 14);
    version[14] = 0;
    return 0;
}

static char ebuf[40];

const char *fuji_strerror(uint8_t rc)
{
    if (rc == FN_E_NODEV) {
        return "NO FUJINET FOUND";
    }
    if (rc == FN_E_NOWIFI) {
        return "THE FUJINET IS NOT ON WI-FI (SET IT UP IN CONFIG)";
    }
    if (rc == FN_E_BUS && fuji_err == SP_BADPCNT) {
        /* Seen with a Yellowstone card: STATUS and CONTROL reach the
         * FujiNet, but READ is refused before it does. */
        return "THIS SMARTPORT CARD CANNOT READ FROM THE FUJINET ($04)";
    }
    if (rc == FN_E_BUS) {
        fmt_hex8(fmt_str(ebuf, "THE FUJINET DID NOT ANSWER, ERROR $"), fuji_err);
        return ebuf;
    }
    if (rc != FN_E_NET) {
        fmt_hex8(fmt_str(ebuf, "ERROR $"), rc);
        return ebuf;
    }
    switch (fuji_err) {
    case 138: case 202:
        return "THE SERVER DID NOT ANSWER IN TIME";
    case 165:
        return "THE FUJINET CANNOT USE THAT URL";
    case 167:
        return "THE SERVER REFUSED IT (ACCESS DENIED)";
    case 170:
        return "THE SERVER REFUSED IT (OR NOT FOUND)";
    case 200:
        return "THE SERVER REFUSED THE CONNECTION";
    case 201: case 203: case 207:
        return "NO NETWORK: CHECK THE FUJINET'S WI-FI";
    case 204: case 211:
        return "THE CONNECTION WAS CUT OFF";
    }
    fmt_u32(fmt_str(ebuf, "THE FUJINET REPORTED ERROR "), fuji_err);
    return ebuf;
}
