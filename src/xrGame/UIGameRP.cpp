#include "stdafx.h"
#include "UIGameRP.h"
#include "ui/UISpawnMenuRP.h"
#include "game_cl_roleplay.h"
#include "ui/UIHelper.h"
#include "ui/UIStatic.h"

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

void CUIGameRP::Init(int stage)
{
	if (stage == 0)
	{
		//shared
		inherited::Init(stage);
		m_pressjump_caption = UIHelper::CreateTextWnd(*m_msgs_xml, "mp_pressjump", m_window);
		m_pressjump_caption->Enable(false);
	}
	else if (stage == 1)
	{
		//unique
	}
	else if (stage == 2)
	{
		//after
		inherited::Init(stage);
	}
}

void CUIGameRP::SetPressJumpMsgCaption(LPCSTR str)
{
	m_pressjump_caption->SetTextST(str);
}
