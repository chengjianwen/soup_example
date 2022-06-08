#ifndef _WS_UTILS_H
#define _WS_UTILS_H

#include <libsoup/soup.h>

typedef struct
{
    const char *playback_device;
    const char *capture_device;
} WsInfo;

void ConnectionInit (SoupWebsocketConnection *,
                     const char *,
                     const char *);

#endif
