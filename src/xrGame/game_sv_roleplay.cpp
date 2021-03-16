#include "stdafx.h"
#include "game_sv_roleplay.h"

game_sv_roleplay::game_sv_roleplay()
{
	m_type = eGameIDRolePlay;
}

game_sv_roleplay::~game_sv_roleplay()
{
}

void game_sv_roleplay::Create(shared_str & options)
{
	inherited::Create(options);
}

void game_sv_roleplay::OnPlayerSelectTeam(NET_Packet& P, ClientID sender)
{
	s16 team = P.r_s16();

	game_PlayerState*	ps = get_id(sender);
	if (!ps) return;
	
	if (ps->team == team) return;

	ps->team = u8(team & 0x00ff);

	KillPlayer(sender, ps->GameID);
	RespawnPlayer(sender, true);
};

void game_sv_roleplay::OnEvent(NET_Packet & tNetPacket, u16 type, u32 time, ClientID sender)
{
	inherited::OnEvent(tNetPacket, type, time, sender);
}
