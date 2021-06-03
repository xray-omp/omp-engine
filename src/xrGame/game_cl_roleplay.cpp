#include "stdafx.h"
#include "game_cl_roleplay.h"
#include "clsid_game.h"
#include "UIGameRP.h"
#include "ui/UISpawnMenuRP.h"
#include "game_base_menu_events.h"
#include "actor_mp_client.h"

game_cl_roleplay::game_cl_roleplay()
{
	m_uTeamCount = (u8)READ_IF_EXISTS(pSettings, r_u32, "roleplay_settings", "team_count", 0);
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

bool game_cl_roleplay::CanRespawn()
{
	CGameObject *pObject = smart_cast<CGameObject*>(Level().CurrentEntity());
	if (!pObject) return false;
	
	// If we are an actor and we are dead
	return !!smart_cast<CActor*>(pObject) &&
					local_player &&
					local_player->testFlag(GAME_PLAYER_FLAG_VERY_VERY_DEAD);
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

void game_cl_roleplay::TrySwitchJumpCaption()
{
	if (g_dedicated_server)
		return;

	if (!m_game_ui->SpawnMenu()->IsShown() && CanRespawn())
	{
		m_game_ui->SetPressJumpMsgCaption("mp_press_jump2start");
	}
	else
	{
		m_game_ui->SetPressJumpMsgCaption(nullptr);
	}
}

void game_cl_roleplay::shedule_Update(u32 dt)
{
	inherited::shedule_Update(dt);

	TryShowSpawnMenu();
	TrySwitchJumpCaption();
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
	if (kTEAM == key)
	{
		if (
			local_player->testFlag(GAME_PLAYER_HAS_ADMIN_RIGHTS) &&
			!m_game_ui->SpawnMenu()->IsShown()
		)
		{
			m_game_ui->HideShownDialogs();
			m_game_ui->SpawnMenu()->ShowDialog(true);
		}
		return true;
	}
	else if (kJUMP == key)
	{
		if (CanRespawn())
		{
			CGameObject* GO = smart_cast<CGameObject*>(Level().CurrentControlEntity());
			NET_Packet P;
			GO->u_EventGen(P, GE_GAME_EVENT, GO->ID());
			P.w_u16(GAME_EVENT_PLAYER_READY);
			GO->u_EventSend(P);
			return true;
		}
		return false;
	}
	return inherited::OnKeyboardPress(key);
}

void game_cl_roleplay::OnSetCurrentControlEntity(CObject * O)
{

}
