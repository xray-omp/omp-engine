
#include "stdafx.h"
#include "DirectPlayClient.h"
#include "BaseClient.h"
#include "ip_address.h"
#include "dp_ids.h"

#pragma warning(push)
#pragma warning(disable:4995)
#include "dxerr.h"
#pragma warning(pop)

XRNETSERVER_API Flags32	psNET_Flags = { 0 };
XRNETSERVER_API int		psNET_ClientUpdate = 30;		// FPS
XRNETSERVER_API int		psNET_ClientPending = 2;
XRNETSERVER_API char	psNET_Name[32] = "Player";

//static	INetLog* pClNetLog = NULL;

//------------------------------------------------------------------------------

HRESULT WINAPI client_net_handler(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage)
{
	DirectPlayClient* C = (DirectPlayClient*)pvUserContext;
	return C->net_Handler(dwMessageType, pMessage);
}

//------------------------------------------------------------------------------

DirectPlayClient::DirectPlayClient(CTimer* tm)
	: inherited(tm)
#ifdef PROFILE_CRITICAL_SECTIONS
	, net_csEnumeration(MUTEX_PROFILE_ID(DirectPlayClient::net_csEnumeration))
#endif // PROFILE_CRITICAL_SECTIONS
{
	NET = NULL;
	net_Address_server = NULL;
	net_Address_device = NULL;

	//pClNetLog = NULL;//xr_new<INetLog>("logs\\net_cl_log.log", timeServer());
}

DirectPlayClient::~DirectPlayClient()
{
	//xr_delete(pClNetLog); pClNetLog = NULL;
}

//------------------------------------------------------------------------------

#pragma region connect / disconnect

bool DirectPlayClient::CreateConnection(ClientConnectionOptions & opt)
{
	// Create the IDirectPlay8Client object.
	HRESULT CoCreateInstanceRes = CoCreateInstance(CLSID_DirectPlay8Client, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Client, (LPVOID*)&NET);
	if (CoCreateInstanceRes != S_OK)
	{
		DXTRACE_ERR("", CoCreateInstanceRes);
		CHK_DX(CoCreateInstanceRes);
	}

	// Initialize IDirectPlay8Client object.
#ifdef DEBUG
	R_CHK(NET->Initialize(this, client_net_handler, 0));
#else 
	R_CHK(NET->Initialize(this, client_net_handler, DPNINITIALIZE_DISABLEPARAMVAL));
#endif
	bool	bSimulator = FALSE;
	if (strstr(Core.Params, "-netsim"))		bSimulator = TRUE;

	// Create our IDirectPlay8Address Device Address, --- Set the SP for our Device Address
	net_Address_device = NULL;
	R_CHK(CoCreateInstance(CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Address, (LPVOID*)&net_Address_device));
	R_CHK(net_Address_device->SetSP(bSimulator ? &CLSID_NETWORKSIMULATOR_DP8SP_TCPIP : &CLSID_DP8SP_TCPIP));

	// Create our IDirectPlay8Address Server Address, --- Set the SP for our Server Address
	WCHAR	ServerNameUNICODE[256];
	R_CHK(MultiByteToWideChar(CP_ACP, 0, opt.server_name, -1, ServerNameUNICODE, 256));

	net_Address_server = NULL;
	R_CHK(CoCreateInstance(CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Address, (LPVOID*)&net_Address_server));
	R_CHK(net_Address_server->SetSP(bSimulator ? &CLSID_NETWORKSIMULATOR_DP8SP_TCPIP : &CLSID_DP8SP_TCPIP));
	R_CHK(net_Address_server->AddComponent(DPNA_KEY_HOSTNAME, ServerNameUNICODE, 2 * u32(wcslen(ServerNameUNICODE) + 1), DPNA_DATATYPE_STRING));
	R_CHK(net_Address_server->AddComponent(DPNA_KEY_PORT, &opt.sv_port, sizeof(opt.sv_port), DPNA_DATATYPE_DWORD));


	// Debug
	// dump_URL		("! cl ",	net_Address_device);
	// dump_URL		("! en ",	net_Address_server);

	// Now set up the Application Description
	DPN_APPLICATION_DESC        dpAppDesc;
	ZeroMemory(&dpAppDesc, sizeof(DPN_APPLICATION_DESC));
	dpAppDesc.dwSize = sizeof(DPN_APPLICATION_DESC);
	dpAppDesc.guidApplication = NET_GUID;

	// Setup client info

	WCHAR	ClientNameUNICODE[256];
	R_CHK(MultiByteToWideChar(CP_ACP, 0, opt.user_name, -1, ClientNameUNICODE, 256));

	{
		DPN_PLAYER_INFO				Pinfo;
		ZeroMemory(&Pinfo, sizeof(Pinfo));
		Pinfo.dwSize = sizeof(Pinfo);
		Pinfo.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
		Pinfo.pwszName = ClientNameUNICODE;

		SClientConnectData			cl_data;
		cl_data.process_id = GetCurrentProcessId();
		xr_strcpy(cl_data.name, opt.user_name);
		xr_strcpy(cl_data.pass, opt.user_pass);

		Pinfo.pvData = &cl_data;
		Pinfo.dwDataSize = sizeof(cl_data);

		R_CHK(NET->SetClientInfo(&Pinfo, 0, 0, DPNSETCLIENTINFO_SYNC));
	}
	if (stricmp(opt.server_name, "localhost") == 0)
	{
		WCHAR	SessionPasswordUNICODE[4096];
		if (xr_strlen(opt.server_pass))
		{
			CHK_DX(MultiByteToWideChar(CP_ACP, 0, opt.server_pass, -1, SessionPasswordUNICODE, 4096));
			dpAppDesc.dwFlags |= DPNSESSION_REQUIREPASSWORD;
			dpAppDesc.pwszPassword = SessionPasswordUNICODE;
		};

		u32 c_port = u32(opt.cl_port);
		HRESULT res = S_FALSE;
		while (res != S_OK)
		{
			R_CHK(net_Address_device->AddComponent(DPNA_KEY_PORT, &c_port, sizeof(c_port), DPNA_DATATYPE_DWORD));
			res = NET->Connect(
				&dpAppDesc,				// pdnAppDesc
				net_Address_server,		// pHostAddr
				net_Address_device,		// pDeviceInfo
				NULL,					// pdnSecurity
				NULL,					// pdnCredentials
				NULL, 0,				// pvUserConnectData/Size
				NULL,					// pvAsyncContext
				NULL,					// pvAsyncHandle
				DPNCONNECT_SYNC);		// dwFlags
			if (res != S_OK)
			{
				//			xr_string res = Debug.error2string(HostSuccess);

				if (opt.bClPortWasSet)
				{
					Msg("! DirectPlayClient : port %d is BUSY!", c_port);
					return FALSE;
				}
				else
				{
					Msg("! DirectPlayClient : port %d is BUSY!", c_port);
				}

				c_port++;
				if (c_port > END_PORT_LAN)
				{
					return FALSE;
				}
			}
			else
			{
				Msg("- DirectPlayClient : created on port %d!", c_port);
			}
		};

		if (res != S_OK) return FALSE;

		// Create ONE node
		HOST_NODE	NODE;
		ZeroMemory(&NODE, sizeof(HOST_NODE));

		// Copy the Host Address
		R_CHK(net_Address_server->Duplicate(&NODE.pHostAddress));

		// Retreive session name
		char desc[4096];
		ZeroMemory(desc, sizeof(desc));
		DPN_APPLICATION_DESC*	dpServerDesc = (DPN_APPLICATION_DESC*)desc;
		DWORD dpServerDescSize = sizeof(desc);
		dpServerDesc->dwSize = sizeof(DPN_APPLICATION_DESC);
		R_CHK(NET->GetApplicationDesc(dpServerDesc, &dpServerDescSize, 0));
		if (!dpServerDesc->dwApplicationReservedDataSize || !dpServerDesc->pvApplicationReservedData)
		{
			OnInvalidHost();
			return FALSE;
		}
		CopyMemory(&m_game_description, dpServerDesc->pvApplicationReservedData,
			dpServerDesc->dwApplicationReservedDataSize);
		if (dpServerDesc->pwszSessionName) {
			string4096				dpSessionName;
			R_CHK(WideCharToMultiByte(CP_ACP, 0, dpServerDesc->pwszSessionName, -1, dpSessionName, sizeof(dpSessionName), 0, 0));
			NODE.dpSessionName = (char*)(&dpSessionName[0]);
		}
		net_Hosts.push_back(NODE);
	}
	else {
		string64						EnumData;
		EnumData[0] = 0;
		xr_strcat(EnumData, "ToConnect");
		DWORD	EnumSize = xr_strlen(EnumData) + 1;
		// We now have the host address so lets enum
		u32 c_port = opt.cl_port;
		HRESULT res = S_FALSE;
		while (res != S_OK && c_port <= END_PORT)
		{
			R_CHK(net_Address_device->AddComponent(DPNA_KEY_PORT, &c_port, sizeof(c_port), DPNA_DATATYPE_DWORD));

			res = NET->EnumHosts(
				&dpAppDesc,				// pApplicationDesc - for unknown reason
				net_Address_server,		// pdpaddrHost
				net_Address_device,		// pdpaddrDeviceInfo
				EnumData, EnumSize,		// pvUserEnumData, size
				10,						// dwEnumCount
				1000,					// dwRetryInterval
				1000,					// dwTimeOut
				NULL,					// pvUserContext
				NULL,					// pAsyncHandle
				DPNENUMHOSTS_SYNC		// dwFlags
			);
			if (res != S_OK)
			{
				//			xr_string res = Debug.error2string(HostSuccess);
				switch (res)
				{
				case DPNERR_INVALIDHOSTADDRESS:
				{
					OnInvalidHost();
					return FALSE;
				}break;
				case DPNERR_SESSIONFULL:
				{
					OnSessionFull();
					return FALSE;
				}break;
				};

				if (opt.bClPortWasSet)
				{
					Msg("! DirectPlayClient : port %d is BUSY!", c_port);
					return FALSE;
				}
#ifdef DEBUG
				else
					Msg("! DirectPlayClient : port %d is BUSY!", c_port);

				//				const char* x = DXGetErrorString9(res);
				string1024 tmp = "";
				DXTRACE_ERR(tmp, res);
#endif				
				c_port++;
			}
			else
			{
				Msg("- DirectPlayClient : created on port %d!", c_port);
			}
		};


		// ****** Connection
		IDirectPlay8Address*        pHostAddress = NULL;
		if (net_Hosts.empty())
		{
			OnInvalidHost();
			return FALSE;
		};

		WCHAR	SessionPasswordUNICODE[4096];
		if (xr_strlen(opt.server_pass))
		{
			CHK_DX(MultiByteToWideChar(CP_ACP, 0, opt.server_pass, -1, SessionPasswordUNICODE, 4096));
			dpAppDesc.dwFlags |= DPNSESSION_REQUIREPASSWORD;
			dpAppDesc.pwszPassword = SessionPasswordUNICODE;
		};

		net_csEnumeration.Enter();
		// real connect
		for (u32 I = 0; I < net_Hosts.size(); I++)
			Msg("* HOST #%d: %s\n", I + 1, *net_Hosts[I].dpSessionName);

		R_CHK(net_Hosts.front().pHostAddress->Duplicate(&pHostAddress));
		// dump_URL		("! c2s ",	pHostAddress);
		res = NET->Connect(
			&dpAppDesc,				// pdnAppDesc
			pHostAddress,			// pHostAddr
			net_Address_device,		// pDeviceInfo
			NULL,					// pdnSecurity
			NULL,					// pdnCredentials
			NULL, 0,				// pvUserConnectData/Size
			NULL,					// pvAsyncContext
			NULL,					// pvAsyncHandle
			DPNCONNECT_SYNC);		// dwFlags
//		R_CHK(res);		
		net_csEnumeration.Leave();
		_RELEASE(pHostAddress);
#ifdef DEBUG	
		//		const char* x = DXGetErrorString9(res);
		string1024 tmp = "";
		DXTRACE_ERR(tmp, res);
#endif
		switch (res)
		{
		case DPNERR_INVALIDPASSWORD:
		{
			OnInvalidPassword();
		}break;
		case DPNERR_SESSIONFULL:
		{
			OnSessionFull();
		}break;
		case DPNERR_CANTCREATEPLAYER:
		{
			Msg("! Error: Can\'t create player");
		}break;
		}
		if (res != S_OK) return FALSE;
	}

	return TRUE;
}

void DirectPlayClient::DestroyConnection()
{
	if (NET)	NET->Close(0);

	// Clean up Host _list_
	net_csEnumeration.Enter();
	for (u32 i = 0; i < net_Hosts.size(); i++) {
		HOST_NODE&	N = net_Hosts[i];
		_RELEASE(N.pHostAddress);
	}
	net_Hosts.clear();
	net_csEnumeration.Leave();

	// Release interfaces
	_SHOW_REF("cl_netADR_Server", net_Address_server);
	_RELEASE(net_Address_server);
	_SHOW_REF("cl_netADR_Device", net_Address_device);
	_RELEASE(net_Address_device);
	_SHOW_REF("cl_netCORE", NET);
	_RELEASE(NET);
}

#pragma endregion

//------------------------------------------------------------------------------

#pragma region recieve

HRESULT	DirectPlayClient::net_Handler(u32 dwMessageType, PVOID pMessage)
{
	switch (dwMessageType)
	{
	case DPN_MSGID_ENUM_HOSTS_RESPONSE:
	{
		PDPNMSG_ENUM_HOSTS_RESPONSE     pEnumHostsResponseMsg;
		const DPN_APPLICATION_DESC*     pDesc;
		// HOST_NODE*                      pHostNode = NULL;
		// WCHAR*                          pwszSession = NULL;

		pEnumHostsResponseMsg = (PDPNMSG_ENUM_HOSTS_RESPONSE)pMessage;
		pDesc = pEnumHostsResponseMsg->pApplicationDescription;

		if (pDesc->dwApplicationReservedDataSize && pDesc->pvApplicationReservedData)
		{
			R_ASSERT(pDesc->dwApplicationReservedDataSize == sizeof(m_game_description));
			CopyMemory(&m_game_description, pDesc->pvApplicationReservedData,
				pDesc->dwApplicationReservedDataSize);
		}

		// Insert each host response if it isn't already present
		net_csEnumeration.Enter();
		bool	bHostRegistered = FALSE;
		for (u32 I = 0; I < net_Hosts.size(); I++)
		{
			HOST_NODE&	N = net_Hosts[I];
			if (pDesc->guidInstance == N.dpAppDesc.guidInstance)
			{
				// This host is already in the list
				bHostRegistered = TRUE;
				break;
			}
		}

		if (!bHostRegistered)
		{
			// This host session is not in the list then so insert it.
			HOST_NODE	NODE;
			ZeroMemory(&NODE, sizeof(HOST_NODE));

			// Copy the Host Address
			R_CHK(pEnumHostsResponseMsg->pAddressSender->Duplicate(&NODE.pHostAddress));
			CopyMemory(&NODE.dpAppDesc, pDesc, sizeof(DPN_APPLICATION_DESC));

			// Null out all the pointers we aren't copying
			NODE.dpAppDesc.pwszSessionName = NULL;
			NODE.dpAppDesc.pwszPassword = NULL;
			NODE.dpAppDesc.pvReservedData = NULL;
			NODE.dpAppDesc.dwReservedDataSize = 0;
			NODE.dpAppDesc.pvApplicationReservedData = NULL;
			NODE.dpAppDesc.dwApplicationReservedDataSize = 0;

			if (pDesc->pwszSessionName) {
				string4096			dpSessionName;
				R_CHK(WideCharToMultiByte(CP_ACP, 0, pDesc->pwszSessionName, -1, dpSessionName, sizeof(dpSessionName), 0, 0));
				NODE.dpSessionName = (char*)(&dpSessionName[0]);
			}

			net_Hosts.push_back(NODE);

		}
		net_csEnumeration.Leave();
	}
	break;

	case DPN_MSGID_RECEIVE:
	{
		PDPNMSG_RECEIVE	pMsg = (PDPNMSG_RECEIVE)pMessage;

		MultipacketReciever::RecievePacket(pMsg->pReceiveData, pMsg->dwReceiveDataSize);
	}
	break;
	case DPN_MSGID_TERMINATE_SESSION:
	{
		PDPNMSG_TERMINATE_SESSION 	pMsg = (PDPNMSG_TERMINATE_SESSION)pMessage;
		char*			m_data = (char*)pMsg->pvTerminateData;
		u32				m_size = pMsg->dwTerminateDataSize;
		HRESULT			m_hResultCode = pMsg->hResultCode;

		net_Disconnected = TRUE;

		if (m_size != 0)
		{
			OnSessionTerminate(m_data);
#ifdef DEBUG				
			Msg("- Session terminated : %s", m_data);
#endif
		}
		else
		{
#ifdef DEBUG
			OnSessionTerminate((::Debug.error2string(m_hResultCode)));
			Msg("- Session terminated : %s", (::Debug.error2string(m_hResultCode)));
#endif
		}
	};
	break;
	default:
	{
#if	1
		LPSTR	msg = "";
		switch (dwMessageType)
		{
		case DPN_MSGID_ADD_PLAYER_TO_GROUP:			msg = "DPN_MSGID_ADD_PLAYER_TO_GROUP"; break;
		case DPN_MSGID_ASYNC_OP_COMPLETE:			msg = "DPN_MSGID_ASYNC_OP_COMPLETE"; break;
		case DPN_MSGID_CLIENT_INFO:					msg = "DPN_MSGID_CLIENT_INFO"; break;
		case DPN_MSGID_CONNECT_COMPLETE:
		{
			PDPNMSG_CONNECT_COMPLETE pMsg = (PDPNMSG_CONNECT_COMPLETE)pMessage;
#ifdef DEBUG
			//					const char* x = DXGetErrorString9(pMsg->hResultCode);
			if (pMsg->hResultCode != S_OK)
			{
				string1024 tmp = "";
				DXTRACE_ERR(tmp, pMsg->hResultCode);
			}
#endif
			if (pMsg->dwApplicationReplyDataSize)
			{
				string256 ResStr = "";
				strncpy_s(ResStr, (char*)(pMsg->pvApplicationReplyData), pMsg->dwApplicationReplyDataSize);
				Msg("Connection result : %s", ResStr);
			}
			else
				msg = "DPN_MSGID_CONNECT_COMPLETE";
		}break;
		case DPN_MSGID_CREATE_GROUP:				msg = "DPN_MSGID_CREATE_GROUP"; break;
		case DPN_MSGID_CREATE_PLAYER:				msg = "DPN_MSGID_CREATE_PLAYER"; break;
		case DPN_MSGID_DESTROY_GROUP: 				msg = "DPN_MSGID_DESTROY_GROUP"; break;
		case DPN_MSGID_DESTROY_PLAYER: 				msg = "DPN_MSGID_DESTROY_PLAYER"; break;
		case DPN_MSGID_ENUM_HOSTS_QUERY:			msg = "DPN_MSGID_ENUM_HOSTS_QUERY"; break;
		case DPN_MSGID_GROUP_INFO:					msg = "DPN_MSGID_GROUP_INFO"; break;
		case DPN_MSGID_HOST_MIGRATE:				msg = "DPN_MSGID_HOST_MIGRATE"; break;
		case DPN_MSGID_INDICATE_CONNECT:			msg = "DPN_MSGID_INDICATE_CONNECT"; break;
		case DPN_MSGID_INDICATED_CONNECT_ABORTED:	msg = "DPN_MSGID_INDICATED_CONNECT_ABORTED"; break;
		case DPN_MSGID_PEER_INFO:					msg = "DPN_MSGID_PEER_INFO"; break;
		case DPN_MSGID_REMOVE_PLAYER_FROM_GROUP:	msg = "DPN_MSGID_REMOVE_PLAYER_FROM_GROUP"; break;
		case DPN_MSGID_RETURN_BUFFER:				msg = "DPN_MSGID_RETURN_BUFFER"; break;
		case DPN_MSGID_SEND_COMPLETE:				msg = "DPN_MSGID_SEND_COMPLETE"; break;
		case DPN_MSGID_SERVER_INFO:					msg = "DPN_MSGID_SERVER_INFO"; break;
		case DPN_MSGID_TERMINATE_SESSION:			msg = "DPN_MSGID_TERMINATE_SESSION"; break;
		default:									msg = "???"; break;
		}
		//Msg("! ************************************ : %s",msg);
#endif
	}
	break;
	}

	return S_OK;
}

#pragma endregion

//------------------------------------------------------------------------------

#pragma region send

void	DirectPlayClient::SendTo_LL(void* data, u32 size, u32 dwFlags, u32 dwTimeout)
{
	// if (psNET_Flags.test(NETFLAG_LOG_CL_PACKETS))
	// {
	// 	if (!pClNetLog)
	// 		pClNetLog = xr_new<INetLog>("logs\\net_cl_log.log", timeServer());
	// 	if (pClNetLog)
	// 		pClNetLog->LogData(timeServer(), data, size);
	// }
	DPN_BUFFER_DESC				desc;

	desc.dwBufferSize = size;
	desc.pBufferData = (BYTE*)data;

	net_Statistic.dwBytesSended += size;

	// verify
	VERIFY(desc.dwBufferSize);
	VERIFY(desc.pBufferData);
	VERIFY(NET);

	DPNHANDLE	hAsync = 0;
	HRESULT		hr = NET->Send(&desc, 1, dwTimeout, 0, &hAsync, dwFlags | DPNSEND_COALESCE);

	//	Msg("- Client::SendTo_LL [%d]", size);
	if (FAILED(hr))
	{
		Msg("! ERROR: Failed to send net-packet, reason: %s", ::Debug.error2string(hr));
		//		const char* x = DXGetErrorString9(hr);
		string1024 tmp = "";
		DXTRACE_ERR(tmp, hr);
	}

	//	UpdateStatistic();
}

bool DirectPlayClient::SendPingMessage(MSYS_PING& clPing)
{
	DPN_BUFFER_DESC					desc;
	DPNHANDLE						hAsync = 0;
	desc.dwBufferSize = sizeof(clPing);
	desc.pBufferData = LPBYTE(&clPing);

	return !FAILED(NET->Send(&desc, 1, 0, 0, &hAsync, net_flags(FALSE, FALSE, TRUE)));
}

#pragma endregion

//------------------------------------------------------------------------------

#pragma region statistic

void DirectPlayClient::UpdateStatistic()
{
	// Query network statistic for this client
	DPN_CONNECTION_INFO	CI;
	ZeroMemory(&CI, sizeof(CI));
	CI.dwSize = sizeof(CI);
	HRESULT hr = NET->GetConnectionInfo(&CI, 0);
	if (FAILED(hr)) return;

	net_Statistic.Update(CI);
}

#pragma endregion

//------------------------------------------------------------------------------

#pragma region pending messages

bool DirectPlayClient::GetPendingMessagesCount(DWORD& dwPending)
{
	HRESULT hr = NET->GetSendQueueInfo(&dwPending, 0, 0);
	return !FAILED(hr);
}

#pragma endregion

//------------------------------------------------------------------------------

#pragma region server address

#include <WINSOCK2.H>
#include <Ws2tcpip.h>

bool DirectPlayClient::GetServerAddress(ip_address & pAddress, DWORD * pPort)
{
	*pPort = 0;
	if (!net_Address_server) return false;

	WCHAR wstrHostname[2048] = { 0 };
	DWORD dwHostNameSize = sizeof(wstrHostname);
	DWORD dwHostNameDataType = DPNA_DATATYPE_STRING;
	CHK_DX(net_Address_server->GetComponentByName(DPNA_KEY_HOSTNAME, wstrHostname, &dwHostNameSize, &dwHostNameDataType));

	string2048				HostName;
	CHK_DX(WideCharToMultiByte(CP_ACP, 0, wstrHostname, -1, HostName, sizeof(HostName), 0, 0));

	hostent* pHostEnt = gethostbyname(HostName);
	char*					localIP;
	localIP = inet_ntoa(*(struct in_addr *)*pHostEnt->h_addr_list);
	pHostEnt = gethostbyname(pHostEnt->h_name);
	localIP = inet_ntoa(*(struct in_addr *)*pHostEnt->h_addr_list);
	pAddress.set(localIP);

	//.	pAddress[0]				= (char)(*(struct in_addr *)*pHostEnt->h_addr_list).s_net;
	//.	pAddress[1]				= (char)(*(struct in_addr *)*pHostEnt->h_addr_list).s_host;
	//.	pAddress[2]				= (char)(*(struct in_addr *)*pHostEnt->h_addr_list).s_lh;
	//.	pAddress[3]				= (char)(*(struct in_addr *)*pHostEnt->h_addr_list).s_impno;

	DWORD dwPort = 0;
	DWORD dwPortSize = sizeof(dwPort);
	DWORD dwPortDataType = DPNA_DATATYPE_DWORD;
	CHK_DX(net_Address_server->GetComponentByName(DPNA_KEY_PORT, &dwPort, &dwPortSize, &dwPortDataType));
	*pPort = dwPort;

	return true;
}

#pragma endregion

//------------------------------------------------------------------------------
