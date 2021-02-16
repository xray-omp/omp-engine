#pragma once
#include "NET_Shared.h"
#include "BaseServer.h"

#pragma warning(push)
#pragma warning(disable:4995)
#include <DPlay/dplay8.h>
#pragma warning(pop)

class XRNETSERVER_API DirectPlayServer : public BaseServer
{
	friend HRESULT WINAPI server_net_handler(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage);

private:
	typedef BaseServer		inherited;

private:
	// DirectPlay
	IDirectPlay8Server*		NET = nullptr;
	IDirectPlay8Address*	net_Address_device = nullptr;

public:
	DirectPlayServer(CTimer* timer, BOOL	Dedicated);
	virtual ~DirectPlayServer();

private:
	HRESULT				        net_Handler(u32 dwMessageType, PVOID pMessage);
	bool					        GetClientAddress(IDirectPlay8Address* pClientAddress, ip_address& Address, DWORD* pPort = NULL);

protected:
	virtual bool			    CreateConnection(GameDescriptionData& game_descr, ServerConnectionOptions& opt) override;
	virtual void			    DestroyConnection() override;

	virtual bool          GetClientPendingMessagesCount(ClientID ID, DWORD& dwPending) override;

	virtual void          _SendTo_LL(ClientID ID, void* data, u32 size, u32 dwFlags = DPNSEND_GUARANTEED, u32 dwTimeout = 0) override;

public:
	virtual bool          GetClientAddress(ClientID ID, ip_address& Address, DWORD* pPort = NULL) override;
	virtual bool          DisconnectClient(IClient* C, LPCSTR Reason) override;

	virtual void          UpdateClientStatistic(IClient* C) override;
};

