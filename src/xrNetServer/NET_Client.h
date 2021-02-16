#pragma once
#include "NET_Shared.h"

#ifdef USE_DIRECT_PLAY
	#include "DirectPlayClient.h"
	#define NET_CLIENT_CLASS DirectPlayClient
#endif // USE_DIRECT_PLAY
