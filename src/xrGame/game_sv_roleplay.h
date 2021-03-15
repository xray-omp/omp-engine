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

	virtual		void				OnEvent(NET_Packet &tNetPacket, u16 type, u32 time, ClientID sender);
};

