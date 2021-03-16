#include "stdafx.h"
#include "UIGameRP.h"
#include "ui/UISpawnMenuRP.h"
#include "game_cl_roleplay.h"

CUIGameRP::CUIGameRP()
{
	m_pUISpawnMenu = xr_new<CUISpawnMenuRP>();
	m_pUISpawnMenu->Init();
}

CUIGameRP::~CUIGameRP()
{
	xr_delete(m_pUISpawnMenu);
}

void CUIGameRP::SetClGame(game_cl_GameState * g)
{
	inherited::SetClGame(g);
	m_game = smart_cast<game_cl_roleplay*>(g);
	R_ASSERT(m_game);
}
