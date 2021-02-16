#include "stdafx.h"
#include "DirectPlayServer.h"
#include "dxerr.h"

#define NET_BANNED_STR                "Player banned by server!"
#define NET_PROTECTED_SERVER_STR      "Access denied by protected server for this player!"
#define NET_NOTFOR_SUBNET_STR         "Your IP does not present in server's subnet"

static const GUID NET_GUID = { 0x218fa8b, 0x515b, 0x4bf2, { 0x9a, 0x5f, 0x2f, 0x7, 0x9d, 0x17, 0x59, 0xf3 } };
static const GUID CLSID_NETWORKSIMULATOR_DP8SP_TCPIP = { 0x8d3f9e5e, 0xa3bd, 0x475b, { 0x9e, 0x49, 0xb0, 0xe7, 0x71, 0x39, 0x14, 0x3c } };

// -----------------------------------------------------------------------------

HRESULT WINAPI server_net_handler(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage)
{
	DirectPlayServer* C = (DirectPlayServer*)pvUserContext;
	return C->net_Handler(dwMessageType, pMessage);
}

// -----------------------------------------------------------------------------

DirectPlayServer::DirectPlayServer(CTimer* timer, BOOL	Dedicated)
	: inherited(timer, Dedicated)
{
	NET = NULL;
	net_Address_device = NULL;
}

DirectPlayServer::~DirectPlayServer()
{

}

// -----------------------------------------------------------------------------

HRESULT	DirectPlayServer::net_Handler(u32 dwMessageType, PVOID pMessage)
{
	// HRESULT     hr = S_OK;

	switch (dwMessageType)
	{
	case DPN_MSGID_ENUM_HOSTS_QUERY:
	{
		PDPNMSG_ENUM_HOSTS_QUERY	msg = PDPNMSG_ENUM_HOSTS_QUERY(pMessage);
		if (0 == msg->dwReceivedDataSize) return S_FALSE;
		if (!stricmp((const char*)msg->pvReceivedData, "ToConnect")) return S_OK;
		if (*((const GUID*)msg->pvReceivedData) != NET_GUID) return S_FALSE;
		if (!OnCL_QueryHost()) return S_FALSE;
		return S_OK;
	}break;
	case DPN_MSGID_CREATE_PLAYER:
	{
		PDPNMSG_CREATE_PLAYER	msg = PDPNMSG_CREATE_PLAYER(pMessage);
		const	u32				max_size = 1024;
		char	bufferData[max_size];
		DWORD	bufferSize = max_size;
		ZeroMemory(bufferData, bufferSize);
		string512				res;

		// retreive info
		DPN_PLAYER_INFO*		Pinfo = (DPN_PLAYER_INFO*)bufferData;
		Pinfo->dwSize = sizeof(DPN_PLAYER_INFO);
		HRESULT _hr = NET->GetClientInfo(msg->dpnidPlayer, Pinfo, &bufferSize, 0);
		if (_hr == DPNERR_INVALIDPLAYER)
		{
			Assign_ServerType(res); //once
			break;	// server player
		}

		CHK_DX(_hr);

		//string64			cname;
		//CHK_DX( WideCharToMultiByte( CP_ACP, 0, Pinfo->pwszName, -1, cname, sizeof(cname) , 0, 0 ) );

		SClientConnectData	cl_data;
		//xr_strcpy( cl_data.name, cname );

		if (Pinfo->pvData && Pinfo->dwDataSize == sizeof(cl_data))
		{
			cl_data = *((SClientConnectData*)Pinfo->pvData);
		}
		cl_data.clientID.set(msg->dpnidPlayer);

		new_client(&cl_data);
	}
	break;
	case DPN_MSGID_DESTROY_PLAYER:
	{
		PDPNMSG_DESTROY_PLAYER	msg = PDPNMSG_DESTROY_PLAYER(pMessage);
		IClient* tmp_client = net_players.GetFoundClient(
			ClientIdSearchPredicate(static_cast<ClientID>(msg->dpnidPlayer))
		);
		if (tmp_client)
		{
			tmp_client->flags.bConnected = FALSE;
			tmp_client->flags.bReconnect = FALSE;
			OnCL_Disconnected(tmp_client);
			// real destroy
			client_Destroy(tmp_client);
		}
	}
	break;
	case DPN_MSGID_RECEIVE:
	{

		PDPNMSG_RECEIVE	pMsg = PDPNMSG_RECEIVE(pMessage);
		void*	m_data = pMsg->pReceiveData;
		u32		m_size = pMsg->dwReceiveDataSize;
		DPNID   m_sender = pMsg->dpnidSender;

		MSYS_PING*	m_ping = (MSYS_PING*)m_data;

		if ((m_size > 2 * sizeof(u32)) && (m_ping->sign1 == 0x12071980) && (m_ping->sign2 == 0x26111975))
		{
			// this is system message
			if (m_size == sizeof(MSYS_PING))
			{
				// ping - save server time and reply
				m_ping->dwTime_Server = TimerAsync(device_timer);
				ClientID ID; ID.set(m_sender);
				BaseServer::SendTo_Buf(ID, m_data, m_size, net_flags(FALSE, FALSE, TRUE, TRUE));
			}
		}
		else
		{
			MultipacketReciever::RecievePacket(pMsg->pReceiveData, pMsg->dwReceiveDataSize, m_sender);
		}
	} break;

	case DPN_MSGID_INDICATE_CONNECT:
	{
		PDPNMSG_INDICATE_CONNECT msg = (PDPNMSG_INDICATE_CONNECT)pMessage;

		ip_address			HAddr;
		GetClientAddress(msg->pAddressPlayer, HAddr);

		if (GetBannedClient(HAddr))
		{
			msg->dwReplyDataSize = sizeof(NET_BANNED_STR);
			msg->pvReplyData = NET_BANNED_STR;
			return					S_FALSE;
		};
		//first connected client is SV_Client so if it is NULL then this server client tries to connect ;)
		if (SV_Client && !m_ip_filter.is_ip_present(HAddr.m_data.data))
		{
			msg->dwReplyDataSize = sizeof(NET_NOTFOR_SUBNET_STR);
			msg->pvReplyData = NET_NOTFOR_SUBNET_STR;
			return					S_FALSE;
		}
	}break;
	}
	return S_OK;
}

// -----------------------------------------------------------------------------

bool DirectPlayServer::CreateConnection(GameDescriptionData & game_descr, ServerConnectionOptions & opt)
{
	HRESULT CoCreateInstanceRes = CoCreateInstance(
		CLSID_DirectPlay8Server,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_IDirectPlay8Server,
		(LPVOID*)&NET
	);

	if (CoCreateInstanceRes != S_OK)
	{
		DXTRACE_ERR(TEXT("Instance could not be created"), CoCreateInstanceRes);
		CHK_DX(CoCreateInstanceRes);
	}

	// Initialize IDirectPlay8Client object.
#ifdef DEBUG
	CHK_DX(NET->Initialize(this, server_net_handler, 0));
#else
	CHK_DX(NET->Initialize(this, server_net_handler, DPNINITIALIZE_DISABLEPARAMVAL));
#endif

	bool bSimulator = FALSE;
	if (strstr(Core.Params, "-netsim"))
	{
		bSimulator = TRUE;
	}

	// Set server-player info
	DPN_APPLICATION_DESC		dpAppDesc;
	DPN_PLAYER_INFO				dpPlayerInfo;
	WCHAR						wszName[] = L"XRay Server";

	ZeroMemory(&dpPlayerInfo, sizeof(DPN_PLAYER_INFO));
	dpPlayerInfo.dwSize = sizeof(DPN_PLAYER_INFO);
	dpPlayerInfo.dwInfoFlags = DPNINFO_NAME;
	dpPlayerInfo.pwszName = wszName;
	dpPlayerInfo.pvData = NULL;
	dpPlayerInfo.dwDataSize = NULL;
	dpPlayerInfo.dwPlayerFlags = 0;

	CHK_DX(NET->SetServerInfo(&dpPlayerInfo, NULL, NULL, DPNSETSERVERINFO_SYNC));

	// Set server/session description
	WCHAR	SessionNameUNICODE[4096];
	CHK_DX(MultiByteToWideChar(CP_ACP, 0, opt.session_name, -1, SessionNameUNICODE, 4096));
	// Set server/session description

	// Now set up the Application Description
	ZeroMemory(&dpAppDesc, sizeof(DPN_APPLICATION_DESC));
	dpAppDesc.dwSize = sizeof(DPN_APPLICATION_DESC);
	dpAppDesc.dwFlags = DPNSESSION_CLIENT_SERVER | DPNSESSION_NODPNSVR;
	dpAppDesc.guidApplication = NET_GUID;
	dpAppDesc.pwszSessionName = SessionNameUNICODE;
	dpAppDesc.dwMaxPlayers = (m_bDedicated) ? (opt.dwMaxPlayers + 2) : (opt.dwMaxPlayers + 1);
	dpAppDesc.pvApplicationReservedData = &game_descr;
	dpAppDesc.dwApplicationReservedDataSize = sizeof(game_descr);

	WCHAR	SessionPasswordUNICODE[4096];
	if (xr_strlen(opt.server_pass))
	{
		CHK_DX(MultiByteToWideChar(CP_ACP, 0, opt.server_pass, -1, SessionPasswordUNICODE, 4096));
		dpAppDesc.dwFlags |= DPNSESSION_REQUIREPASSWORD;
		dpAppDesc.pwszPassword = SessionPasswordUNICODE;
	};

	// Create our IDirectPlay8Address Device Address, --- Set the SP for our Device Address
	net_Address_device = NULL;
	CHK_DX(CoCreateInstance(CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Address, (LPVOID*)&net_Address_device));
	CHK_DX(net_Address_device->SetSP(bSimulator ? &CLSID_NETWORKSIMULATOR_DP8SP_TCPIP : &CLSID_DP8SP_TCPIP));

	DWORD dwTraversalMode = DPNA_TRAVERSALMODE_NONE;
	CHK_DX(net_Address_device->AddComponent(DPNA_KEY_TRAVERSALMODE, &dwTraversalMode, sizeof(dwTraversalMode), DPNA_DATATYPE_DWORD));

	HRESULT HostSuccess = S_FALSE;
	// We are now ready to host the app and will try different ports
	psNET_Port = opt.dwServerPort;
	while (HostSuccess != S_OK)
	{
		CHK_DX(net_Address_device->AddComponent(DPNA_KEY_PORT, &psNET_Port, sizeof(psNET_Port), DPNA_DATATYPE_DWORD));

		HostSuccess = NET->Host
		(
			&dpAppDesc,				// AppDesc
			&net_Address_device, 1, // Device Address
			NULL, NULL,             // Reserved
			NULL,                   // Player Context
			0);						// dwFlags
		if (HostSuccess != S_OK)
		{
			if (opt.bPortWasSet)
			{
				Msg("! DirectPlayServer : port %d is BUSY!", psNET_Port);
				return false; //ErrConnect;
			}
			else
			{
				Msg("! DirectPlayServer : port %d is BUSY!", psNET_Port);
			}

			psNET_Port++;
			if (psNET_Port > END_PORT_LAN)
			{
				return false; ErrConnect;
			}
		}
		else
		{
			Msg("- DirectPlayServer : created on port %d!", psNET_Port);
		}
	};

	CHK_DX(HostSuccess);

	return true;
}

// -----------------------------------------------------------------------------

void DirectPlayServer::DestroyConnection()
{
	if (NET)	NET->Close(0);

	// Release interfaces
	_RELEASE(net_Address_device);
	_RELEASE(NET);
}

// -----------------------------------------------------------------------------

void DirectPlayServer::_SendTo_LL(ClientID ID, void * data, u32 size, u32 dwFlags, u32 dwTimeout)
{
	// send it
	DPN_BUFFER_DESC		desc;
	desc.dwBufferSize = size;
	desc.pBufferData = LPBYTE(data);

	// verify
	VERIFY(desc.dwBufferSize);
	VERIFY(desc.pBufferData);

	DPNHANDLE	hAsync = 0;
	HRESULT		_hr = NET->SendTo(
		ID.value(),
		&desc, 1,
		dwTimeout,
		0, &hAsync,
		dwFlags | DPNSEND_COALESCE
	);

	if (SUCCEEDED(_hr) || (DPNERR_CONNECTIONLOST == _hr))	return;

	R_CHK(_hr);
}

// -----------------------------------------------------------------------------

void DirectPlayServer::UpdateClientStatistic(IClient* C)
{
	// Query network statistic for this client
	DPN_CONNECTION_INFO			CI;
	ZeroMemory(&CI, sizeof(CI));
	CI.dwSize = sizeof(CI);
	if (!psNET_direct_connect)
	{
		HRESULT hr = NET->GetConnectionInfo(C->ID.value(), &CI, 0);
		if (FAILED(hr))				return;
	}
	C->stats.Update(CI);
}

// -----------------------------------------------------------------------------


bool DirectPlayServer::GetClientAddress(ClientID ID, ip_address& Address, DWORD* pPort)
{
	IDirectPlay8Address* pClAddr = NULL;
	CHK_DX(NET->GetClientAddress(ID.value(), &pClAddr, 0));

	return GetClientAddress(pClAddr, Address, pPort);
};


bool DirectPlayServer::GetClientAddress(IDirectPlay8Address* pClientAddress, ip_address& Address, DWORD* pPort)
{
	WCHAR				wstrHostname[256] = { 0 };
	DWORD dwSize = sizeof(wstrHostname);
	DWORD dwDataType = 0;
	CHK_DX(pClientAddress->GetComponentByName(DPNA_KEY_HOSTNAME, wstrHostname, &dwSize, &dwDataType));

	string256				HostName;
	CHK_DX(WideCharToMultiByte(CP_ACP, 0, wstrHostname, -1, HostName, sizeof(HostName), 0, 0));

	Address.set(HostName);

	if (pPort != NULL)
	{
		DWORD dwPort = 0;
		DWORD dwPortSize = sizeof(dwPort);
		DWORD dwPortDataType = DPNA_DATATYPE_DWORD;
		CHK_DX(pClientAddress->GetComponentByName(DPNA_KEY_PORT, &dwPort, &dwPortSize, &dwPortDataType));
		*pPort = dwPort;
	};
	return true;
};

// -----------------------------------------------------------------------------

bool DirectPlayServer::DisconnectClient(IClient* C, LPCSTR Reason)
{
	if (!C) return false;

	HRESULT res = NET->DestroyClient(C->ID.value(), Reason, xr_strlen(Reason) + 1, 0);
	CHK_DX(res);
	return true;
}

// -----------------------------------------------------------------------------

bool DirectPlayServer::GetClientPendingMessagesCount(ClientID ID, DWORD & dwPending)
{
	HRESULT hr = NET->GetSendQueueInfo(ID.value(), &dwPending, 0, 0);
	return !FAILED(hr);
}

// -----------------------------------------------------------------------------
