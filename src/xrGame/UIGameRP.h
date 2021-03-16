#pragma once
#include "UIGameFMP.h"

class game_cl_roleplay;
class CUISpawnMenuRP;

class CUIGameRP : public CUIGameFMP
{
	typedef CUIGameFMP inherited;

private:
	game_cl_roleplay *m_game;
	CUISpawnMenuRP   *m_pUISpawnMenu;
	CUITextWnd       *m_pressjump_caption;

public:
	CUIGameRP();
	virtual ~CUIGameRP();
	virtual void Init(int stage);
	virtual void SetClGame(game_cl_GameState* g);

	IC CUISpawnMenuRP* SpawnMenu() { return m_pUISpawnMenu; }
	 	
	void SetPressJumpMsgCaption(LPCSTR str);
};

