#pragma once
#include "game_cl_freemp.h"

class CUIGameRP;

class game_cl_roleplay : public game_cl_freemp
{
	typedef game_cl_freemp inherited;

private:
	CUIGameRP									*m_game_ui;
	u8												m_uTeamCount = 0;

protected:
	bool											m_bTeamSelected = false;

private:
	void											TryShowSpawnMenu();
	void											TrySwitchJumpCaption();

protected:
	bool											CanRespawn();

public:
														game_cl_roleplay();
	virtual										~game_cl_roleplay();

	virtual u8								GetTeamCount() { return m_uTeamCount; };

	virtual CUIGameCustom*		createGameUI();
	virtual void							SetGameUI(CUIGameCustom*);

	virtual void							OnConnected();

	virtual void							shedule_Update(u32 dt);

	virtual void							OnTeamSelect(int team);

	virtual bool							OnKeyboardPress(int key);

	virtual void							OnSetCurrentControlEntity(CObject *O);
};

