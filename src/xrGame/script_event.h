#pragma once
#include "script_export_space.h"

class ScriptEvent
{
public:
	int SenderID;
	NET_Packet Packet;
};

typedef class_exporter<ScriptEvent> ScriptEventExporter;
add_to_type_list(ScriptEventExporter)
#undef script_type_list
#define script_type_list save_type_list(ScriptEventExporter)
