#pragma once
#include "NET_Shared.h"

#ifdef USE_DIRECT_PLAY
	#include "DirectPlayServer.h"
	#define NET_SERVER_CLASS DirectPlayServer
#endif // USE_DIRECT_PLAY
