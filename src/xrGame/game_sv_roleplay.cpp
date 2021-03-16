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

void game_sv_roleplay::OnEvent(NET_Packet & tNetPacket, u16 type, u32 time, ClientID sender)
{
	inherited::OnEvent(tNetPacket, type, time, sender);
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


void game_sv_roleplay::OnPlayerReady(ClientID id_who)
{
	// process GAME_EVENT_PLAYER_READY
	inherited::OnPlayerReady(id_who);
}

void game_sv_roleplay::RespawnPlayer(ClientID id_who, bool NoSpectator)
{
	inherited::RespawnPlayer(id_who, NoSpectator);

	xrClientData*	xrCData = (xrClientData*)m_server->ID_to_client(id_who);
	if (!xrCData) return;

	game_PlayerState*	ps = xrCData->ps;
	if (!ps) return;

	CSE_ALifeCreatureActor	*pA = smart_cast<CSE_ALifeCreatureActor*>(xrCData->owner);
	if (!pA) return;
	
	SpawnWeapon4Actor(pA->ID, "mp_players_rukzak", 0, ps->pItemList);
}

BOOL game_sv_roleplay::OnTouch(u16 eid_who, u16 eid_what, BOOL bForced)
{
	CSE_ActorMP *e_who = smart_cast<CSE_ActorMP*>(m_server->ID_to_entity(eid_who));
	if (!e_who)
		return TRUE;

	CSE_Abstract *e_entity = m_server->ID_to_entity(eid_what);
	if (!e_entity)
		return FALSE;

	// pick up players bag
	if (e_entity->m_tClassID == CLSID_OBJECT_PLAYERS_BAG)
	{
		return OnTouchPlayersBag(e_who, e_entity);
	}

	return TRUE;
}

void game_sv_roleplay::OnDetach(u16 eid_who, u16 eid_what)
{
	CSE_ActorMP *e_who = smart_cast<CSE_ActorMP*>(m_server->ID_to_entity(eid_who));
	if (!e_who)
		return;

	CSE_Abstract *e_entity = m_server->ID_to_entity(eid_what);
	if (!e_entity)
		return;
	
	// drop players bag
	if (e_entity->m_tClassID == CLSID_OBJECT_PLAYERS_BAG)
	{
		OnDetachPlayersBag(e_who, e_entity);
	}
}
