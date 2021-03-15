#pragma once
#include "UIGameFMP.h"

class game_cl_roleplay;

class CUIGameRP : public CUIGameFMP
{
	typedef CUIGameFMP inherited;

private:
	game_cl_roleplay *m_game;

public:
	CUIGameRP();
	virtual ~CUIGameRP();

	virtual void SetClGame(game_cl_GameState* g);
};

