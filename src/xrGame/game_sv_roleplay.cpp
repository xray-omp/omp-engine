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

void game_sv_roleplay::OnEvent(NET_Packet & tNetPacket, u16 type, u32 time, ClientID sender)
{
	inherited::OnEvent(tNetPacket, type, time, sender);
}
