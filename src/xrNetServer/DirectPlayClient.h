#pragma once
#include "BaseClient.h"

class XRNETSERVER_API DirectPlayClient : public BaseClient
{
	friend HRESULT WINAPI client_net_handler(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage);

	typedef BaseClient inherited;

private:
	xrCriticalSection		net_csEnumeration;

protected:
	// Direct play

	struct HOST_NODE //deprecated...
	{
		DPN_APPLICATION_DESC	dpAppDesc;
		IDirectPlay8Address*	pHostAddress;
		shared_str				dpSessionName;
	};

	IDirectPlay8Client*		NET;
	IDirectPlay8Address*	net_Address_device;
	IDirectPlay8Address*	net_Address_server;
	xr_vector<HOST_NODE>	net_Hosts;

public:
	DirectPlayClient(CTimer* tm);
	virtual ~DirectPlayClient();

private:
	HRESULT					  net_Handler(u32 dwMessageType, PVOID pMessage);

protected:
	virtual bool      IsConnectionInit() override { return NET != nullptr; }

	virtual bool			CreateConnection(ClientConnectionOptions& opt) override;
	virtual void			DestroyConnection() override;
	virtual	void			SendTo_LL(void* data, u32 size, u32 dwFlags = DPNSEND_GUARANTEED, u32 dwTimeout = 0) override;

	virtual bool      GetPendingMessagesCount(DWORD& dwPending) override;
	virtual bool      SendPingMessage(MSYS_PING& clPing) override;

public:
	virtual	bool			GetServerAddress(ip_address& pAddress, DWORD* pPort)  override;

	virtual bool      HasSessionName() override { return !net_Hosts.empty(); }
	virtual LPCSTR    net_SessionName() const override { return *(net_Hosts.front().dpSessionName); }

	// statistic
	virtual	void			UpdateStatistic()  override;
};

