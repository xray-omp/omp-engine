#include "pch_script.h"
#include "script_event.h"

using namespace luabind;

#pragma optimize("s",on)
void ScriptEventExporter::script_register(lua_State *L)
{
	module(L)
		[
			class_<ScriptEvent>("script_event")
			.def(constructor<>())
		.def_readwrite("SenderID", &ScriptEvent::SenderID)
		.def_readwrite("Packet", &ScriptEvent::Packet)
		];
}
