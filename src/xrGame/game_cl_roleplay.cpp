#include "stdafx.h"
#include "game_cl_roleplay.h"
#include "clsid_game.h"
#include "UIGameRP.h"
#include "ui/UISpawnMenuRP.h"
#include "game_base_menu_events.h"
#include "actor_mp_client.h"

game_cl_roleplay::game_cl_roleplay()
{
	m_uTeamCount = 4; // TEMP
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

void game_cl_roleplay::TryShowSpawnMenu()
{
	if (g_dedicated_server)
		return;

	if (!m_bTeamSelected && !m_game_ui->SpawnMenu()->IsShown())
	{
		m_game_ui->SpawnMenu()->ShowDialog(true);
	}
}

void game_cl_roleplay::shedule_Update(u32 dt)
{
	inherited::shedule_Update(dt);

	TryShowSpawnMenu();
}

void game_cl_roleplay::OnTeamSelect(int team)
{
	CGameObject *pObject = smart_cast<CGameObject*>(Level().CurrentEntity());
	if (!pObject) return;

	NET_Packet P;
	pObject->u_EventGen(P, GE_GAME_EVENT, pObject->ID());
	P.w_u16(GAME_EVENT_PLAYER_GAME_MENU);
	P.w_u8(PLAYER_CHANGE_TEAM);

	P.w_s16(static_cast<s16>(team));
	pObject->u_EventSend(P);
	m_bTeamSelected = true;
}

bool game_cl_roleplay::OnKeyboardPress(int key)
{
	if (kTEAM == key) // TODO: remove
	{
		if (!m_game_ui->SpawnMenu()->IsShown())
		{
			m_game_ui->SpawnMenu()->ShowDialog(true);
		}
		return true;
	}
	return inherited::OnKeyboardPress(key);
}

void game_cl_roleplay::OnSetCurrentControlEntity(CObject * O)
{

}
