#pragma once
#include "game_cl_mp.h"

class CUIGameFMP;

class game_cl_freemp :public game_cl_mp
{
private:
	typedef game_cl_mp inherited;
	CUIGameFMP *m_game_ui;

public:
			game_cl_freemp();
	virtual	~game_cl_freemp();

	virtual LPCSTR		GetTeamColor(u32 /*team*/) const { return "%c[255,255,240,190]"; }
	virtual u32				GetTeamColor_u32(u32 /*team*/) const { return color_rgba(255, 240, 190, 255); }

	virtual CUIGameCustom* createGameUI();
	virtual void SetGameUI(CUIGameCustom*);


	virtual	void net_import_state(NET_Packet& P);
	virtual	void net_import_update(NET_Packet& P);
	
	virtual void shedule_Update(u32 dt);

	virtual	bool OnKeyboardPress(int key);

	virtual LPCSTR GetGameScore(string32&	score_dest);
	virtual bool Is_Rewarding_Allowed()  const { return false; };

	virtual void OnConnected();

};

