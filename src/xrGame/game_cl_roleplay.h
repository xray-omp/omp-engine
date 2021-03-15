#pragma once
#include "game_cl_freemp.h"

class CUIGameRP;

class game_cl_roleplay : public game_cl_freemp
{
	typedef game_cl_freemp inherited;

private:
	CUIGameRP									*m_game_ui;

public:
														game_cl_roleplay();
	virtual										~game_cl_roleplay();

	virtual CUIGameCustom*		createGameUI();
	virtual void							SetGameUI(CUIGameCustom*);

	virtual void							OnConnected();

	virtual void							shedule_Update(u32 dt);
};

