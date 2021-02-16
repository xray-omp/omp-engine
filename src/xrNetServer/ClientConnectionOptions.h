#pragma once

struct ClientConnectionOptions
{
	string256 server_name = "";
	string64 password_str = "";
	string64 user_name_str = "";
	string64 user_pass = "";
	int psSV_Port = 0;
	int psCL_Port = 0;
	bool bClPortWasSet = false;
};


