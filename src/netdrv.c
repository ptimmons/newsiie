/*
 * Loading the network driver. See netdrv.h; netcall.s passes the app's
 * calls on to it.
 *
 * Built with NEWSIIE_SHOTS (make shotsdisk), the status field shows a
 * placeholder address when no card is found, for the manual's screenshots,
 * which come from an emulator with no card; the manual says so.
 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "netdrv.h"
#include "fmt.h"
#include "ui.h"

/* The slot: both drivers' memory areas start and end here. */
extern uint8_t _DRVCOGS_START__[], _DRVCOGS_SIZE__[];
#define SLOT_SIZE   ((uint16_t)_DRVCOGS_SIZE__)

/* Load a driver file into the slot and ask it for its device. Its BSS,
 * which follows the bytes in the file, is cleared first. */
static uint8_t load(const char *name)
{
    int fd, n;

    fd = open(name, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    n = read(fd, _DRVCOGS_START__, SLOT_SIZE);
    close(fd);
    if (n < NETDRV_HEAD || _DRVCOGS_START__[0] != NETDRV_MAGIC ||
        *(void **)(_DRVCOGS_START__ + 1) != _ONCE_RUN__) {
        return 0;
    }
    memset(_DRVCOGS_START__ + n, 0, SLOT_SIZE - (uint16_t)n);
    return net_find();
}

void net_init(void)
{
    if (load("NEWSIIE.COGS") || load("NEWSIIE.FUJI")) {
        net_loaded = 0x80;
    }
}

void net_none_check(void)
{
#ifdef NEWSIIE_SHOTS
    fmt_str(status_net, "COGS 192.168.1.60");
#else
    fmt_str(status_net, "NO NETWORK CARD");
#endif
}

const char *net_none_error(uint8_t rc)
{
    (void)rc;
    return "NO COGS CARD OR FUJINET FOUND";
}
