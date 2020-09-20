////////////////////////////////////////////////////////////////////////////
//	Module 		: script_binder.cpp
//	Created 	: 26.03.2004
//  Modified 	: 26.03.2004
//	Author		: Dmitriy Iassenev
//	Description : Script objects binder
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "ai_space.h"
#include "script_engine.h"
#include "script_binder.h"
#include "xrServer_Objects_ALife.h"
#include "script_binder_object.h"
#include "script_game_object.h"
#include "gameobject.h"
#include "level.h"

// comment next string when commiting
//#define DBG_DISABLE_SCRIPTS

CScriptBinder::CScriptBinder		()
{
	init					();
}

CScriptBinder::~CScriptBinder		()
{
	VERIFY					(!m_object);
}

void CScriptBinder::init			()
{
	m_object				= 0;
}

void CScriptBinder::clear			()
{
	try {
		xr_delete			(m_object);
	}
	catch(...) {
		m_object			= 0;
	}
	init					();
}

void CScriptBinder::reinit			()
{
#ifdef DEBUG_MEMORY_MANAGER
	u32									start = 0;
	if (g_bMEMO)
		start							= Memory.mem_usage();
#endif // DEBUG_MEMORY_MANAGER
	if (m_object) {
		try {
			m_object->reinit	();
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during reinit", m_object->m_object->Name());
			clear			();
		}
	}
#ifdef DEBUG_MEMORY_MANAGER
	if (g_bMEMO) {
//		lua_gc				(ai().script_engine().lua(),LUA_GCCOLLECT,0);
//		lua_gc				(ai().script_engine().lua(),LUA_GCCOLLECT,0);
		Msg					("CScriptBinder::reinit() : %d",Memory.mem_usage() - start);
	}
#endif // DEBUG_MEMORY_MANAGER
}

void CScriptBinder::Load			(LPCSTR section)
{
}

void CScriptBinder::reload			(LPCSTR section)
{
#ifdef DEBUG_MEMORY_MANAGER
	u32									start = 0;
	if (g_bMEMO)
		start							= Memory.mem_usage();
#endif // DEBUG_MEMORY_MANAGER
#ifndef DBG_DISABLE_SCRIPTS
	VERIFY					(!m_object);
	if (!pSettings->line_exist(section,"script_binding"))
		return;
	
	luabind::functor<void>	lua_function;
	if (!ai().script_engine().functor(pSettings->r_string(section,"script_binding"),lua_function)) {
		ai().script_engine().script_log	(ScriptStorage::eLuaMessageTypeError,"function %s is not loaded!",pSettings->r_string(section,"script_binding"));
		return;
	}
	
	CGameObject				*game_object = smart_cast<CGameObject*>(this);

	try {
		lua_function		(game_object ? game_object->lua_game_object() : 0);
	}
	catch(...) {
		R_ASSERT3(0, "Script binder crashed during getting lua game object", m_object->m_object->Name());
		clear				();
		return;
	}

	if (m_object) {
		try {
			m_object->reload(section);
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during reload", m_object->m_object->Name());
			clear			();
		}
	}
#endif
#ifdef DEBUG_MEMORY_MANAGER
	if (g_bMEMO) {
//		lua_gc				(ai().script_engine().lua(),LUA_GCCOLLECT,0);
//		lua_gc				(ai().script_engine().lua(),LUA_GCCOLLECT,0);
		Msg					("CScriptBinder::reload() : %d",Memory.mem_usage() - start);
	}
#endif // DEBUG_MEMORY_MANAGER
}

BOOL CScriptBinder::net_Spawn		(CSE_Abstract* DC)
{
#ifdef DEBUG_MEMORY_MANAGER
	u32									start = 0;
	if (g_bMEMO)
		start							= Memory.mem_usage();
#endif // DEBUG_MEMORY_MANAGER
	CSE_Abstract			*abstract = (CSE_Abstract*)DC;
	CSE_ALifeObject			*object = smart_cast<CSE_ALifeObject*>(abstract);
	if (object && m_object) {
		try {
			return			((BOOL)m_object->net_Spawn(object));
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during net_Spawn", m_object->m_object->Name());
			clear			();
		}
	}

#ifdef DEBUG_MEMORY_MANAGER
	if (g_bMEMO) {
//		lua_gc				(ai().script_engine().lua(),LUA_GCCOLLECT,0);
//		lua_gc				(ai().script_engine().lua(),LUA_GCCOLLECT,0);
		Msg					("CScriptBinder::net_Spawn() : %d",Memory.mem_usage() - start);
	}
#endif // DEBUG_MEMORY_MANAGER

	return					(TRUE);
}

void CScriptBinder::net_Destroy		()
{
	if (m_object) {
#ifdef _DEBUG
		Msg						("* Core object %s is UNbinded from the script object",smart_cast<CGameObject*>(this) ? *smart_cast<CGameObject*>(this)->cName() : "");
#endif // _DEBUG
		try {
			m_object->net_Destroy	();
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during net_Destroy", m_object->m_object->Name());
			clear			();
		}
	}
	xr_delete				(m_object);
}

void CScriptBinder::set_object		(CScriptBinderObject *object)
{
	if (/*OnServer()*/ true) {
		R_ASSERT2				(!m_object,"Cannot bind to the object twice!");
#ifdef _DEBUG
		Msg					("* Core object %s is binded with the script object",smart_cast<CGameObject*>(this) ? *smart_cast<CGameObject*>(this)->cName() : "");
#endif // _DEBUG
		m_object			= object;
	} else {
		xr_delete			(object);
	}
}

void CScriptBinder::shedule_Update	(u32 time_delta)
{
	if (m_object) {
		try {
			m_object->shedule_Update	(time_delta);
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during shedule_Update", m_object->m_object->Name());
			clear			();
		}
	}
}

void CScriptBinder::save			(NET_Packet &output_packet)
{
	if (m_object) {
		try {
			m_object->save	(&output_packet);
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during save", m_object->m_object->Name());
			clear			();
		}
	}
}

void CScriptBinder::load			(IReader &input_packet)
{
	if (m_object) {
		try {
			m_object->load	(&input_packet);
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during load", m_object->m_object->Name());
			clear			();
		}
	}
}

BOOL CScriptBinder::net_SaveRelevant()
{
	if (m_object) {
		try {
			return			(m_object->net_SaveRelevant());
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during net_SaveRelevant", m_object->m_object->Name());
			clear			();
		}
	}
	return							(FALSE);
}

void CScriptBinder::net_Relcase		(CObject *object)
{
	CGameObject						*game_object = smart_cast<CGameObject*>(object);
	if (m_object && game_object) {
		try {
			m_object->net_Relcase	(game_object->lua_game_object());
		}
		catch(...) {
			R_ASSERT3(0, "Script binder crashed during net_Relcase", m_object->m_object->Name());
			clear			();
		}
	}
}
