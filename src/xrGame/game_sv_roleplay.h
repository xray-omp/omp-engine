#pragma once
#include "game_sv_freemp.h"

class game_sv_roleplay : public game_sv_freemp
{
	typedef game_sv_freemp inherited;

public:
												game_sv_roleplay();
	virtual								~game_sv_roleplay();

	virtual LPCSTR				type_name() const { return "roleplay"; };

	virtual void					Create(shared_str &options);

	virtual		void				OnPlayerSelectTeam(NET_Packet& P, ClientID sender);
	virtual		void				OnPlayerReady(ClientID id_who);
	virtual		void				RespawnPlayer(ClientID id_who, bool NoSpectator);

	virtual		void				OnEvent(NET_Packet &tNetPacket, u16 type, u32 time, ClientID sender);
	
	virtual		BOOL				OnTouch(u16 eid_who, u16 eid_what, BOOL bForced = FALSE);
	virtual		void				OnDetach(u16 eid_who, u16 eid_what);

	// drop items after death
	virtual		void				FillDeathActorRejectItems(CSE_ActorMP *actor, xr_vector<CSE_Abstract*> & to_reject);
						BOOL				OnTouchPlayersBag(CSE_ActorMP *actor, CSE_Abstract *item);
						void				OnDetachPlayersBag(CSE_ActorMP *actor, CSE_Abstract *item);
};

