#pragma once
#include "PHNetState.h"

#define MIN_LINEAR_VELOCITY_COMPONENT -32.f
#define MAX_LINEAR_VELOCITY_COMPONENT 32.f

struct net_physics_state
{
	Fvector physics_linear_velocity;
	Fvector physics_position;
	bool	physics_state_enabled;
	u32		dwTimeStamp;

	net_physics_state();
	void fill(SPHNetState &state, u32 time);
	void write(NET_Packet &packet);
	void read(NET_Packet &packet);
};
