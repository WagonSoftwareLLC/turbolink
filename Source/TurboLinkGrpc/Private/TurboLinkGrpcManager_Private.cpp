// Copyright (C) Developed by Neo Jin. All Rights Reserved.
#include "TurboLinkGrpcManager_Private.h"
#include "TurboLinkGrpcModule.h"

EGrpcServiceState UTurboLinkGrpcManager::Private::GrpcStateToServiceState(grpc_connectivity_state State)
{
	switch (State)
	{
	case GRPC_CHANNEL_IDLE:
		return EGrpcServiceState::Idle;

	case GRPC_CHANNEL_CONNECTING:
		return EGrpcServiceState::Connecting;

	case GRPC_CHANNEL_READY:
		return EGrpcServiceState::Ready;

	case GRPC_CHANNEL_TRANSIENT_FAILURE:
		return EGrpcServiceState::TransientFailure;

	case GRPC_CHANNEL_SHUTDOWN:
		return EGrpcServiceState::Shutdown;
	}

	return EGrpcServiceState::NotCreate;
}

std::shared_ptr<UTurboLinkGrpcManager::Private::ServiceChannel> UTurboLinkGrpcManager::Private::CreateServiceChannel(const char* EndPoint, UGrpcService* AttachedService)
{
	auto itServiceChannel = ChannelMap.find(EndPoint);
	if (itServiceChannel != ChannelMap.end())
	{
		itServiceChannel->second->AttachedServices.insert(AttachedService);
		return itServiceChannel->second;
	}

	std::shared_ptr<ServiceChannel> channel = std::make_shared<ServiceChannel>();

	//get config instance
	FTurboLinkGrpcModule* turboLinkModule = FModuleManager::GetModulePtr<FTurboLinkGrpcModule>("TurboLinkGrpc");
	const UTurboLinkGrpcConfig* config = turboLinkModule->GetTurboLinkGrpcConfig();

	//apply channel arguments
	grpc::ChannelArguments args;
	args.SetInt(GRPC_ARG_ENABLE_RETRIES, config->EnableRetries ? 1: 0);
	args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, config->KeepAliveTime);
	args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, config->KeepAliveTimeOut);
	args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, config->KeepAlivePermitWithoutCalls ? 1 : 0);
	args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, config->Http2MaxPingsWithoutData);

	//is server-side tls mode?
	if (config->EnableServerSideTLS)
	{
		FString ServerRootCerts = config->GetServerRootCerts();

		grpc::SslCredentialsOptions ssl_opts;
		ssl_opts.pem_root_certs = TCHAR_TO_UTF8(*ServerRootCerts);

		// If we are on Windows, we need to fetch the root certificates from the system registry
		// otherwise grpc source will try to default to a linux system path that doesn't exist
#if PLATFORM_WINDOWS
		if (ServerRootCerts.IsEmpty())
		{
			ssl_opts = getSslOptions();
		}
#endif
		channel->RpcChannel = grpc::CreateCustomChannel(EndPoint, grpc::SslCredentials(ssl_opts), args);
	}
	else
	{
		//Insecure mode
		channel->RpcChannel = grpc::CreateCustomChannel(EndPoint, grpc::InsecureChannelCredentials(), args);
	}

	channel->EndPoint = EndPoint;
	channel->AttachedServices.insert(AttachedService);
	channel->ChannelState = channel->RpcChannel->GetState(false);
	ChannelMap.insert(std::make_pair(EndPoint, channel));
	return channel;
}

void UTurboLinkGrpcManager::Private::RemoveServiceChannel(std::shared_ptr<ServiceChannel> Channel, UGrpcService* AttachedService)
{
	auto itServiceChannel = ChannelMap.find(Channel->EndPoint);
	if (itServiceChannel == ChannelMap.end() || itServiceChannel->second != Channel)
	{
		return;
	}
	//Detach GrpcService
	Channel->AttachedServices.erase(AttachedService);

	//Still another GrpcService working
	if (!(Channel->AttachedServices.empty())) return;

	//Empty Channel, shutdown
	Channel->RpcChannel = nullptr;
	ChannelMap.erase(Channel->EndPoint);
}

std::unique_ptr<grpc::ClientContext> UTurboLinkGrpcManager::Private::CreateRpcClientContext(void)
{
    std::unique_ptr<grpc::ClientContext> context = std::make_unique<grpc::ClientContext>();
    return context;
}

void UTurboLinkGrpcManager::Private::ShutdownCompletionQueue()
{
	if (CompletionQueue == nullptr) return;

	//Shutdown!
	CompletionQueue->Shutdown();

	//Remove all event
	void* got_tag;
	bool ok;

	gpr_timespec deadline;
	deadline.clock_type = GPR_CLOCK_MONOTONIC;
	deadline.tv_nsec = 1;
	deadline.tv_sec = 0;
	auto result = CompletionQueue->AsyncNext(&got_tag, &ok, deadline);

	while (result != grpc::CompletionQueue::NextStatus::SHUTDOWN)
	{
		result = CompletionQueue->AsyncNext(&got_tag, &ok, deadline);
	}
}


#if PLATFORM_WINDOWS
#include <wincrypt.h>
grpc::SslCredentialsOptions UTurboLinkGrpcManager::Private::getSslOptions()
{
	// Fetch root certificate as required on Windows (s. issue 25533).
	grpc::SslCredentialsOptions result;

	// Open root certificate store.
	HANDLE hRootCertStore = CertOpenSystemStoreW(NULL, L"ROOT");
	if (!hRootCertStore)
		return result;

	// Get all root certificates.
	PCCERT_CONTEXT pCert = NULL;
	while ((pCert = CertEnumCertificatesInStore(hRootCertStore, pCert)) != NULL)
	{
		// Append this certificate in PEM formatted data.
		DWORD size = 0;
		CryptBinaryToStringW(pCert->pbCertEncoded, pCert->cbCertEncoded, CRYPT_STRING_BASE64HEADER,
			NULL, &size);
		std::vector<WCHAR> pem(size);
		CryptBinaryToStringW(pCert->pbCertEncoded, pCert->cbCertEncoded, CRYPT_STRING_BASE64HEADER,
			pem.data(), &size);

		result.pem_root_certs += utf8Encode(pem.data());
	}

	CertCloseStore(hRootCertStore, 0);
	return result;
}

std::string UTurboLinkGrpcManager::Private::utf8Encode(const std::wstring& wstr)
{
	if (wstr.empty())
		return std::string();

	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], sizeNeeded, NULL, NULL);
	return strTo;
}
#endif