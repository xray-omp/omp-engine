#include "stdafx.h"
#include "UIGameRP.h"
#include "game_cl_roleplay.h"

CUIGameRP::CUIGameRP()
{
}

CUIGameRP::~CUIGameRP()
{
}

void CUIGameRP::SetClGame(game_cl_GameState * g)
{
	inherited::SetClGame(g);
	m_game = smart_cast<game_cl_roleplay*>(g);
	R_ASSERT(m_game);
}
