/*
 * The FujiNet build's view of the network: found or not, on Wi-Fi or not,
 * and its address, as a status-bar field. See net.c for the CoGS build's.
 */
#include "fuji.h"
#include "fmt.h"
#include "ui.h"

void net_check(void)
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
