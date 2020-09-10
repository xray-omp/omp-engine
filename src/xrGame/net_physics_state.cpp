#include "StdAfx.h"
#include "net_physics_state.h"

net_physics_state::net_physics_state()
{
	physics_linear_velocity.set(0,0,0);
	physics_position.set(0, 0, 0);
	physics_state_enabled = false;
	dwTimeStamp = 0;
}

void net_physics_state::fill(SPHNetState &state, u32 time)
{
	dwTimeStamp = time;
	physics_linear_velocity = state.linear_vel;
	physics_position = state.position;
	physics_state_enabled = state.enabled;
}

void net_physics_state::write(NET_Packet &packet)
{
	// time
	packet.w_u32(dwTimeStamp);

	// linear velocity
	//clamp(physics_linear_velocity.x, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);
	//clamp(physics_linear_velocity.y, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);
	//clamp(physics_linear_velocity.z, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);
	//
	//packet.w_float_q8(physics_linear_velocity.x, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);
	//packet.w_float_q8(physics_linear_velocity.y, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);
	//packet.w_float_q8(physics_linear_velocity.z, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);

	// position
	packet.w_float(physics_position.x);
	packet.w_float(physics_position.y);
	packet.w_float(physics_position.z);

	// physics state enabled
	packet.w_u8(physics_state_enabled ? 1 : 0);
}

void net_physics_state::read(NET_Packet &packet)
{
	// time
	packet.r_u32(dwTimeStamp);

	// linear velocity
	//packet.r_float_q8(physics_linear_velocity.x, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);
	//packet.r_float_q8(physics_linear_velocity.y, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);
	//packet.r_float_q8(physics_linear_velocity.z, MIN_LINEAR_VELOCITY_COMPONENT, MAX_LINEAR_VELOCITY_COMPONENT);

	// position
	packet.r_float(physics_position.x);
	packet.r_float(physics_position.y);
	packet.r_float(physics_position.z);

	// physics state enabled
	physics_state_enabled = static_cast<bool>(packet.r_u8());
}
