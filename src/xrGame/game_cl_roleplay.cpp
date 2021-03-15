#include "stdafx.h"
#include "game_cl_roleplay.h"
#include "clsid_game.h"
#include "UIGameRP.h"

game_cl_roleplay::game_cl_roleplay()
{
}

game_cl_roleplay::~game_cl_roleplay()
{
}

CUIGameCustom * game_cl_roleplay::createGameUI()
{
	if (g_dedicated_server)
		return NULL;

	CLASS_ID clsid = CLSID_GAME_UI_ROLEPLAY;
	m_game_ui = smart_cast<CUIGameRP*> (NEW_INSTANCE(clsid));
	R_ASSERT(m_game_ui);
	m_game_ui->Load();
	m_game_ui->SetClGame(this);
	return m_game_ui;
}

void game_cl_roleplay::SetGameUI(CUIGameCustom *uigame)
{
	inherited::SetGameUI(uigame);
	m_game_ui = smart_cast<CUIGameRP*>(uigame);
	R_ASSERT(m_game_ui);
}

void game_cl_roleplay::OnConnected()
{
	inherited::OnConnected();
	m_game_ui = smart_cast<CUIGameRP*>(CurrentGameUI());
}

void game_cl_roleplay::shedule_Update(u32 dt)
{
	inherited::shedule_Update(dt);
}
