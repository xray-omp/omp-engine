#include "stdafx.h"
#include "UISpawnMenuRP.h"
#include <dinput.h>
#include "UIXmlInit.h"
#include "../level.h"
#include "../game_cl_roleplay.h"
#include "UIStatix.h"
#include "UIScrollView.h"
#include "UI3tButton.h"
#include "../xr_level_controller.h"
#include "uicursor.h"
#include "uigamecustom.h"
#include "game_cl_roleplay.h"

CUISpawnMenuRP::CUISpawnMenuRP()
{
	game_cl_mp* game = smart_cast<game_cl_mp*>(&Game());
	team_buttons_count = game->GetTeamCount();

	m_pBackground = xr_new<CUIStatic>();
	m_pBackground->SetAutoDelete(true);
	AttachChild(m_pBackground);

	m_pCaption = xr_new<CUIStatic>();
	m_pCaption->SetAutoDelete(true);
	AttachChild(m_pCaption);

	m_pTextDesc = xr_new<CUIScrollView>();
	m_pTextDesc->SetAutoDelete(true);
	AttachChild(m_pTextDesc);

	for (u8 i = 0; i < team_buttons_count; i++)
	{
		m_pFrames.push_back(xr_new<CUIStatic>());
		AttachChild(m_pFrames.back());
		m_pImages.push_back(xr_new<CUIStatix>());
		AttachChild(m_pImages.back());
	}
}

CUISpawnMenuRP::~CUISpawnMenuRP()
{
	for (u32 i = 0; i < m_pFrames.size(); i++)
		xr_delete(m_pFrames[i]);

	for (u32 i = 0; i < m_pImages.size(); i++)
		xr_delete(m_pImages[i]);
}

void CUISpawnMenuRP::Init()
{
	CUIXml xml_doc;
	xml_doc.Load(CONFIG_PATH, UI_PATH, "spawn_roleplay.xml");

	CUIXmlInit::InitWindow(xml_doc, "team_selector", 0, this);
	CUIXmlInit::InitStatic(xml_doc, "team_selector:caption", 0, m_pCaption);
	CUIXmlInit::InitStatic(xml_doc, "team_selector:background", 0, m_pBackground);
	CUIXmlInit::InitScrollView(xml_doc, "team_selector:text_desc", 0, m_pTextDesc);

	string32 node;
	for (u8 i = 0; i < team_buttons_count; i++)
	{
		xr_sprintf(node, "team_selector:image_frames_%d", i);
		CUIXmlInit::InitStatic(xml_doc, node, 0, m_pFrames[i]);

		xr_sprintf(node, "team_selector:image_%d", i);
		CUIXmlInit::InitStatic(xml_doc, node, 0, m_pImages[i]);
	}
}

void CUISpawnMenuRP::SendMessage(CUIWindow *pWnd, s16 msg, void *pData)
{
	if (BUTTON_CLICKED == msg)
	{
		game_cl_roleplay * game = smart_cast<game_cl_roleplay*>(&Game());
		R_ASSERT(game);
		for (u8 i = 0; i < team_buttons_count; i++)
		{
			if (pWnd == m_pImages[i])
				game->OnTeamSelect(i + 1);
		}
		HideDialog();
	}
}