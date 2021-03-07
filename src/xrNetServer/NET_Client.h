#pragma once
#include "NET_Shared.h"

#ifdef USE_DIRECT_PLAY
#include "DirectPlayClient.h"
#define NET_CLIENT_CLASS DirectPlayClient
#else
#include "SteamNetClient.h"
#define NET_CLIENT_CLASS SteamNetClient
#endif // USE_DIRECT_PLAY
