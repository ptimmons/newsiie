/*
 * The FujiNet driver's view of the network: found or not, on Wi-Fi or not,
 * and its address, as a status-bar field. Part of NEWSIIE.FUJI; the app
 * calls it as net_check() (netdrv.h). See net.c for the CoGS card's.
 */
#include "fuji.h"
#include "fmt.h"
#include "ui.h"
#include "netdrv.h"

void fuji_net_check(void)
{
    uint8_t ip[4];
    char version[15];
    char *p;

    if (!fuji_find()) {
        fmt_str(status_net, "NO FUJINET");
        return;
    }
    p = fmt_str(status_net, "FUJINET ");
    if (!fuji_wifi()) {
        fmt_str(p, "NO WI-FI");
    } else if (fuji_config(ip, version) || !(ip[0] | ip[1] | ip[2] | ip[3])) {
        fmt_str(p, "READY");
    } else {
        fmt_ip(p, ip);
    }
}
