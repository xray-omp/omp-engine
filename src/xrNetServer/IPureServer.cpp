#include "stdafx.h"
#include "IPureServer.h"
#include "NET_Log.h"
#include "IBannedClient.h"
#include "dxerr.h"

#include "../xrGameSpy/xrGameSpy_MainDefs.h"


#define NET_BANNED_STR                "Player banned by server!"
#define NET_PROTECTED_SERVER_STR      "Access denied by protected server for this player!"
#define NET_NOTFOR_SUBNET_STR		  "Your IP does not present in server's subnet"

XRNETSERVER_API ClientID BroadcastCID(0xffffffff);
XRNETSERVER_API int psNET_ServerUpdate = 30; // FPS
XRNETSERVER_API int psNET_ServerPending = 3;

static	INetLog* pSvNetLog = NULL;

static const GUID NET_GUID = { 0x218fa8b, 0x515b, 0x4bf2, { 0x9a, 0x5f, 0x2f, 0x7, 0x9d, 0x17, 0x59, 0xf3 } };
static const GUID CLSID_NETWORKSIMULATOR_DP8SP_TCPIP = { 0x8d3f9e5e, 0xa3bd, 0x475b, { 0x9e, 0x49, 0xb0, 0xe7, 0x71, 0x39, 0x14, 0x3c } };

// -----------------------------------------------------------------------------

static HRESULT WINAPI Handler(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage)
{
	IPureServer* C = (IPureServer*)pvUserContext;
	return C->net_Handler(dwMessageType, pMessage);
}

// -----------------------------------------------------------------------------

LPCSTR IPureServer::GetBannedListName()
{
	return "banned_list_ip.ltx";
}

IClient* IPureServer::ID_to_client(ClientID ID, bool ScanAll)
{
	if (0 == ID.value())			return NULL;
	IClient* ret_client = GetClientByID(ID);
	if (ret_client || !ScanAll)
		return ret_client;

	return NULL;
}

void IPureServer::_Recieve(const void* data, u32 data_size, u32 param)
{
	if (data_size >= NET_PacketSizeLimit) {
		Msg("! too large packet size[%d] received, DoS attack?", data_size);
		return;
	}

	NET_Packet  packet;
	ClientID    id;

	id.set(param);
	packet.construct(data, data_size);

	csMessage.Enter();

	if (psNET_Flags.test(NETFLAG_LOG_SV_PACKETS))
	{
		if (!pSvNetLog)
			pSvNetLog = xr_new<INetLog>("logs\\net_sv_log.log", TimeGlobal(device_timer));

		if (pSvNetLog)
			pSvNetLog->LogPacket(TimeGlobal(device_timer), &packet, TRUE);
	}

	u32	result = OnMessage(packet, id);

	csMessage.Leave();

	if (result)
		SendBroadcast(id, packet, result);
}

//==============================================================================

IPureServer::IPureServer(CTimer* timer, BOOL	Dedicated)
	: m_bDedicated(Dedicated)
#ifdef PROFILE_CRITICAL_SECTIONS
	, csPlayers(MUTEX_PROFILE_ID(IPureServer::csPlayers))
	, csMessage(MUTEX_PROFILE_ID(IPureServer::csMessage))
#endif // PROFILE_CRITICAL_SECTIONS
{
	device_timer = timer;
	stats.clear();
	stats.dwSendTime = TimeGlobal(device_timer);
	SV_Client = NULL;
	NET = NULL;
	net_Address_device = NULL;
	pSvNetLog = NULL;//xr_new<INetLog>("logs\\net_sv_log.log", TimeGlobal(device_timer));

#ifdef DEBUG
	sender_functor_invoked = false;
#endif
}

IPureServer::~IPureServer()
{
	for (u32 it = 0; it < BannedAddresses.size(); it++)
		xr_delete(BannedAddresses[it]);

	BannedAddresses.clear();

	SV_Client = NULL;

	xr_delete(pSvNetLog);

	psNET_direct_connect = FALSE;
}

IPureServer::EConnect IPureServer::Connect(LPCSTR options, GameDescriptionData & game_descr)
{
	connect_options = options;
	psNET_direct_connect = FALSE;

	if (strstr(options, "/single"))
		psNET_direct_connect = TRUE;

	// Parse options
	SvConnectionOptions connectOpt;
	
	// SESSION NAME	
	xr_strcpy(connectOpt.session_name, options); //sertanly we can use game_descr structure for determinig level_name,
												 // but for backward compatibility we save next line...
	if (strchr(connectOpt.session_name, '/'))	*strchr(connectOpt.session_name, '/') = 0;

	// PASSWORD
	if (strstr(options, "psw="))
	{
		const char* PSW = strstr(options, "psw=") + 4;
		if (strchr(PSW, '/'))
			strncpy_s(connectOpt.password_str, PSW, strchr(PSW, '/') - PSW);
		else
			strncpy_s(connectOpt.password_str, PSW, 63);
	}

	// MAX PLAYERS
	if (strstr(options, "maxplayers="))
	{
		const char* sMaxPlayers = strstr(options, "maxplayers=") + 11;
		string64 tmpStr = "";
		if (strchr(sMaxPlayers, '/'))
			strncpy_s(tmpStr, sMaxPlayers, strchr(sMaxPlayers, '/') - sMaxPlayers);
		else
			strncpy_s(tmpStr, sMaxPlayers, 63);
		connectOpt.dwMaxPlayers = atol(tmpStr);
	}

	if (connectOpt.dwMaxPlayers > 32 || connectOpt.dwMaxPlayers < 1)
	{
		connectOpt.dwMaxPlayers = 32;
	}
	
	// SERVER PORT
	connectOpt.bPortWasSet = false;
	connectOpt.dwServerPort = START_PORT_LAN_SV;
	if (strstr(options, "portsv="))
	{
		const char* ServerPort = strstr(options, "portsv=") + 7;
		string64 tmpStr = "";
		if (strchr(ServerPort, '/'))
			strncpy_s(tmpStr, ServerPort, strchr(ServerPort, '/') - ServerPort);
		else
			strncpy_s(tmpStr, ServerPort, 63);
		connectOpt.dwServerPort = atol(tmpStr);
		clamp(connectOpt.dwServerPort, u32(START_PORT), u32(END_PORT));
		connectOpt.bPortWasSet = true; //this is not casual game
	}

	if (!psNET_direct_connect)
	{
		bool success = Connect_DP(game_descr, connectOpt);
		if (!success)
		{
			return ErrNoError;
		}
		BannedList_Load();
		IpList_Load();
	}
	
	return ErrNoError;
}




bool IPureServer::Connect_DP(GameDescriptionData & game_descr, SvConnectionOptions& opt)
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
	CHK_DX(NET->Initialize(this, Handler, 0));
#else
	CHK_DX(NET->Initialize(this, Handler, DPNINITIALIZE_DISABLEPARAMVAL));
#endif

	BOOL bSimulator = FALSE;
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
	if (xr_strlen(opt.password_str))
	{
		CHK_DX(MultiByteToWideChar(CP_ACP, 0, opt.password_str, -1, SessionPasswordUNICODE, 4096));
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
				Msg("! IPureServer : port %d is BUSY!", psNET_Port);
				return false; //ErrConnect;
			}
			else
			{
				Msg("! IPureServer : port %d is BUSY!", psNET_Port);
			}

			psNET_Port++;
			if (psNET_Port > END_PORT_LAN)
			{
				return false; ErrConnect;
			}
		}
		else
		{
			Msg("- IPureServer : created on port %d!", psNET_Port);
		}
	};

	CHK_DX(HostSuccess);

	return true;
}



void IPureServer::Disconnect()
{
	//.	config_Save		();

	if (!psNET_direct_connect)
	{
		BannedList_Save();
		IpList_Unload();
	}

	if (NET)	NET->Close(0);

	// Release interfaces
	_RELEASE(net_Address_device);
	_RELEASE(NET);
}

HRESULT	IPureServer::net_Handler(u32 dwMessageType, PVOID pMessage)
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
				//						IPureServer::SendTo_LL	(ID,m_data,m_size,net_flags(FALSE,FALSE,TRUE));
				IPureServer::SendTo_Buf(ID, m_data, m_size, net_flags(FALSE, FALSE, TRUE, TRUE));
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

void	IPureServer::Flush_Clients_Buffers()
{
#if NET_LOG_PACKETS
	Msg("#flush server send-buf");
#endif

	struct LocalSenderFunctor
	{
		static void FlushBuffer(IClient* client)
		{
			client->MultipacketSender::FlushSendBuffer(0);
		}
	};

	net_players.ForEachClientDo(
		LocalSenderFunctor::FlushBuffer
	);
}

void	IPureServer::SendTo_Buf(ClientID id, void* data, u32 size, u32 dwFlags, u32 dwTimeout)
{
	IClient* tmp_client = net_players.GetFoundClient(
		ClientIdSearchPredicate(id));
	VERIFY(tmp_client);
	tmp_client->MultipacketSender::SendPacket(data, size, dwFlags, dwTimeout);
}


void	IPureServer::SendTo_LL(ClientID ID/*DPNID ID*/, void* data, u32 size, u32 dwFlags, u32 dwTimeout)
{
	//	if (psNET_Flags.test(NETFLAG_LOG_SV_PACKETS)) pSvNetLog->LogData(TimeGlobal(device_timer), data, size);
	if (psNET_Flags.test(NETFLAG_LOG_SV_PACKETS))
	{
		if (!pSvNetLog) pSvNetLog = xr_new<INetLog>("logs\\net_sv_log.log", TimeGlobal(device_timer));
		if (pSvNetLog) pSvNetLog->LogData(TimeGlobal(device_timer), data, size);
	}

	// send it
	DPN_BUFFER_DESC		desc;
	desc.dwBufferSize = size;
	desc.pBufferData = LPBYTE(data);

#ifdef _DEBUG
	u32 time_global = TimeGlobal(device_timer);
	if (time_global - stats.dwSendTime >= 999)
	{
		stats.dwBytesPerSec = (stats.dwBytesPerSec * 9 + stats.dwBytesSended) / 10;
		stats.dwBytesSended = 0;
		stats.dwSendTime = time_global;
	};
	if (ID.value())
		stats.dwBytesSended += size;
#endif

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


	//	Msg("- IPureServer::SendTo_LL [%d]", size);

	if (SUCCEEDED(_hr) || (DPNERR_CONNECTIONLOST == _hr))	return;

	R_CHK(_hr);

}

void	IPureServer::SendTo(ClientID ID/*DPNID ID*/, NET_Packet& P, u32 dwFlags, u32 dwTimeout)
{
	SendTo_LL(ID, P.B.data, P.B.count, dwFlags, dwTimeout);
}

void	IPureServer::SendBroadcast_LL(ClientID exclude, void* data, u32 size, u32 dwFlags)
{
	struct ClientExcluderPredicate
	{
		ClientID id_to_exclude;
		ClientExcluderPredicate(ClientID exclude) :
			id_to_exclude(exclude)
		{}
		bool operator()(IClient* client)
		{
			if (client->ID == id_to_exclude)
				return false;
			if (!client->flags.bConnected)
				return false;
			return true;
		}
	};
	struct ClientSenderFunctor
	{
		IPureServer*	m_owner;
		void*			m_data;
		u32				m_size;
		u32				m_dwFlags;
		ClientSenderFunctor(IPureServer* owner, void* data, u32 size, u32 dwFlags) :
			m_owner(owner), m_data(data), m_size(size), m_dwFlags(dwFlags)
		{}
		void operator()(IClient* client)
		{
			m_owner->SendTo_LL(client->ID, m_data, m_size, m_dwFlags);
		}
	};
	ClientSenderFunctor temp_functor(this, data, size, dwFlags);
	net_players.ForFoundClientsDo(ClientExcluderPredicate(exclude), temp_functor);
}

void	IPureServer::SendBroadcast(ClientID exclude, NET_Packet& P, u32 dwFlags)
{
	// Perform broadcasting
	SendBroadcast_LL(exclude, P.B.data, P.B.count, dwFlags);
}

u32	IPureServer::OnMessage(NET_Packet& P, ClientID sender)	// Non-Zero means broadcasting with "flags" as returned
{
	/*
	u16 m_type;
	P.r_begin	(m_type);
	switch (m_type)
	{
	case M_CHAT:
		{
			char	buffer[256];
			P.r_string(buffer);
			printf	("RECEIVE: %s\n",buffer);
		}
		break;
	}
	*/

	return 0;
}

void IPureServer::OnCL_Connected(IClient* CL)
{
	Msg("* Player 0x%08x connected.\n", CL->ID.value());
}
void IPureServer::OnCL_Disconnected(IClient* CL)
{
	Msg("* Player 0x%08x disconnected.\n", CL->ID.value());
}

BOOL IPureServer::HasBandwidth(IClient* C)
{
	u32	dwTime = TimeGlobal(device_timer);
	u32	dwInterval = 0;

	if (psNET_direct_connect)
	{
		UpdateClientStatistic(C);
		C->dwTime_LastUpdate = dwTime;
		dwInterval = 1000;
		return					TRUE;
	}

	if (psNET_ServerUpdate != 0) dwInterval = 1000 / psNET_ServerUpdate;
	if (psNET_Flags.test(NETFLAG_MINIMIZEUPDATES))	dwInterval = 1000;	// approx 2 times per second

	HRESULT hr;
	if (psNET_ServerUpdate != 0 && (dwTime - C->dwTime_LastUpdate) > dwInterval)
	{
		// check queue for "empty" state
		DWORD				dwPending;
		hr = NET->GetSendQueueInfo(C->ID.value(), &dwPending, 0, 0);
		if (FAILED(hr))		return FALSE;

		if (dwPending > u32(psNET_ServerPending))
		{
			C->stats.dwTimesBlocked++;
			return FALSE;
		};

		UpdateClientStatistic(C);
		// ok
		C->dwTime_LastUpdate = dwTime;
		return TRUE;
	}
	return FALSE;
}

void	IPureServer::UpdateClientStatistic(IClient* C)
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

void	IPureServer::ClearStatistic()
{
	stats.clear();
	struct StatsClearFunctor
	{
		static void Clear(IClient* client)
		{
			client->stats.Clear();
		}
	};
	net_players.ForEachClientDo(StatsClearFunctor::Clear);
};

/*bool			IPureServer::DisconnectClient	(IClient* C)
{
	if (!C) return false;

	string64 Reason = "st_kicked_by_server";
	HRESULT res = NET->DestroyClient(C->ID.value(), Reason, xr_strlen(Reason)+1, 0);
	CHK_DX(res);
	return true;
}*/

bool			IPureServer::DisconnectClient(IClient* C, LPCSTR Reason)
{
	if (!C) return false;

	HRESULT res = NET->DestroyClient(C->ID.value(), Reason, xr_strlen(Reason) + 1, 0);
	CHK_DX(res);
	return true;
}

bool	IPureServer::DisconnectAddress(const ip_address& Address, LPCSTR reason)
{
	u32 players_count = net_players.ClientsCount();
	buffer_vector<IClient*>	PlayersToDisconnect(
		_alloca(players_count * sizeof(IClient*)),
		players_count
	);
	struct ToDisconnectFillerFunctor
	{
		IPureServer*				m_owner;
		buffer_vector<IClient*>*	dest;
		ip_address const*			address_to_disconnect;
		ToDisconnectFillerFunctor(IPureServer* owner, buffer_vector<IClient*>* dest_disconnect, ip_address const* address) :
			m_owner(owner), dest(dest_disconnect), address_to_disconnect(address)
		{}
		void operator()(IClient* client)
		{
			ip_address			tmp_address;
			m_owner->GetClientAddress(client->ID, tmp_address);
			if (*address_to_disconnect == tmp_address)
			{
				dest->push_back(client);
			};
		}
	};
	ToDisconnectFillerFunctor tmp_functor(this, &PlayersToDisconnect, &Address);
	net_players.ForEachClientDo(tmp_functor);

	buffer_vector<IClient*>::iterator it = PlayersToDisconnect.begin();
	buffer_vector<IClient*>::iterator it_e = PlayersToDisconnect.end();

	for (; it != it_e; ++it)
	{
		DisconnectClient(*it, reason);
	}
	return true;
}

bool IPureServer::GetClientAddress(IDirectPlay8Address* pClientAddress, ip_address& Address, DWORD* pPort)
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

bool IPureServer::GetClientAddress(ClientID ID, ip_address& Address, DWORD* pPort)
{
	IDirectPlay8Address* pClAddr = NULL;
	CHK_DX(NET->GetClientAddress(ID.value(), &pClAddr, 0));

	return GetClientAddress(pClAddr, Address, pPort);
};

IBannedClient*	IPureServer::GetBannedClient(const ip_address& Address)
{
	for (u32 it = 0; it < BannedAddresses.size(); it++)
	{
		IBannedClient* pBClient = BannedAddresses[it];
		if (pBClient->HAddr == Address)
			return pBClient;
	}
	return NULL;
};

void IPureServer::BanClient(IClient* C, u32 BanTime)
{
	ip_address				ClAddress;
	GetClientAddress(C->ID, ClAddress);
	BanAddress(ClAddress, BanTime);
};

void IPureServer::BanAddress(const ip_address& Address, u32 BanTimeSec)
{
	if (GetBannedClient(Address))
	{
		Msg("Already banned\n");
		return;
	};

	IBannedClient* pNewClient = xr_new<IBannedClient>();
	pNewClient->HAddr = Address;
	time(&pNewClient->BanTime);
	pNewClient->BanTime += BanTimeSec;
	if (pNewClient)
	{
		BannedAddresses.push_back(pNewClient);
		BannedList_Save();
	}
};

void IPureServer::UnBanAddress(const ip_address& Address)
{
	if (!GetBannedClient(Address))
	{
		Msg("! Can't find address %s in ban list.", Address.to_string().c_str());
		return;
	};

	for (u32 it = 0; it < BannedAddresses.size(); it++)
	{
		IBannedClient* pBClient = BannedAddresses[it];
		if (pBClient->HAddr == Address)
		{
			xr_delete(BannedAddresses[it]);
			BannedAddresses.erase(BannedAddresses.begin() + it);
			Msg("Unbanning %s", Address.to_string().c_str());
			BannedList_Save();
			break;
		}
	};
};

void IPureServer::Print_Banned_Addreses()
{
	Msg("- ----banned ip list begin-------");
	for (u32 i = 0; i < BannedAddresses.size(); i++)
	{
		IBannedClient* pBClient = BannedAddresses[i];
		Msg("- %s to %s", pBClient->HAddr.to_string().c_str(), pBClient->BannedTimeTo().c_str());
	}
	Msg("- ----banned ip list end-------");
}

void IPureServer::BannedList_Save()
{
	string_path					temp;
	FS.update_path(temp, "$app_data_root$", GetBannedListName());

	CInifile					ini(temp, FALSE, FALSE, TRUE);

	for (u32 it = 0; it < BannedAddresses.size(); it++)
	{
		IBannedClient* cl = BannedAddresses[it];
		cl->Save(ini);
	};
}

void IPureServer::BannedList_Load()
{
	string_path					temp;
	FS.update_path(temp, "$app_data_root$", GetBannedListName());

	CInifile					ini(temp);

	CInifile::RootIt it = ini.sections().begin();
	CInifile::RootIt it_e = ini.sections().end();

	for (; it != it_e; ++it)
	{
		const shared_str& sect_name = (*it)->Name;
		IBannedClient* Cl = xr_new<IBannedClient>();
		Cl->Load(ini, sect_name);
		BannedAddresses.push_back(Cl);
	}
}

void IPureServer::IpList_Load()
{
	Msg("* Initializing IP filter.");
	m_ip_filter.load();
}
void IPureServer::IpList_Unload()
{
	Msg("* Deinitializing IP filter.");
	m_ip_filter.unload();
}
bool IPureServer::IsPlayerIPDenied(u32 ip_address)
{
	return !m_ip_filter.is_ip_present(ip_address);
}

bool banned_client_comparer(IBannedClient* C1, IBannedClient* C2)
{
	return C1->BanTime > C2->BanTime;
}

void IPureServer::UpdateBannedList()
{
	if (!BannedAddresses.size())		return;
	std::sort(BannedAddresses.begin(), BannedAddresses.end(), banned_client_comparer);
	time_t						T;
	time(&T);

	IBannedClient* Cl = BannedAddresses.back();
	if (Cl->BanTime < T)
	{
		ip_address				Address = Cl->HAddr;
		UnBanAddress(Address);
	}
}
