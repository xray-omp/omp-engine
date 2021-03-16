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

public:
	CUIGameRP();
	virtual ~CUIGameRP();

	IC CUISpawnMenuRP* SpawnMenu() { return m_pUISpawnMenu; }

	virtual void SetClGame(game_cl_GameState* g);
};

