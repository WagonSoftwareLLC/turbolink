// Copyright (C) Developed by Neo Jin. All Rights Reserved.
#include "TurboLinkGrpcClient.h"
#include "TurboLinkGrpcContext.h"
#include "TurboLinkGrpcModule.h"

UGrpcClient::UGrpcClient()
{
	UE_LOG(LogTurboLink, Log, TEXT("Construct GrpcClient"));
}

UGrpcClient::~UGrpcClient()
{
	UE_LOG(LogTurboLink, Log, TEXT("Destruct GrpcClient"));
}

void UGrpcClient::AddContext(TSharedPtr<GrpcContext> Context)
{
	ContextMap.Add(Context->GetHandle(), Context);
}

void UGrpcClient::RemoveContext(const FGrpcContextHandle& Handle)
{
	ContextMap.Remove(Handle);
}

TSharedPtr<GrpcContext>* UGrpcClient::GetContext(const FGrpcContextHandle& Handle)
{
	TSharedPtr<GrpcContext>* context = ContextMap.Find(Handle);
	return context;
}

EGrpcContextState UGrpcClient::GetContextState(const FGrpcContextHandle& Handle) const
{
	const TSharedPtr<GrpcContext>* context = ContextMap.Find(Handle);
	if (context == nullptr) return EGrpcContextState::Done;

	return (*context)->GetState();
}

bool UGrpcClient::AddMetadataToContext(const FGrpcContextHandle& Handle, const FString& Key, const FString& Value)
{
	const TSharedPtr<GrpcContext>* context = ContextMap.Find(Handle);
	if (context == nullptr) return false;

	(*context)->RpcContext->AddMetadata(TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
	return true;
}

void UGrpcClient::TryCancelContext(const FGrpcContextHandle& Handle)
{
	const TSharedPtr<GrpcContext>* context = ContextMap.Find(Handle);
	if (context == nullptr) return;

	(*context)->TryCancel();
}

void UGrpcClient::Tick(float DeltaTime)
{
	//Remove all context that has done.
	if (ContextMap.Num() > 0)
	{
		TArray<uint32> contextHandleArray;
		ContextMap.GetKeys(contextHandleArray);
		for (auto handle : contextHandleArray)
		{
			auto context = ContextMap[handle];
			if (context->GetState() == EGrpcContextState::Done)
			{
				ContextMap.Remove(handle);
			}
		}
		ContextMap.Compact();
	}
}

void UGrpcClient::Shutdown()
{
	//already in shutdown progress?
	if (bIsShutdowning) return;

	OnContextStateChange.Clear();
	//Close all context
	for (auto contextIter : ContextMap)
	{
		TSharedPtr<GrpcContext> context = contextIter.Value;
		if (context->GetState()== EGrpcContextState::Busy || context->GetState() == EGrpcContextState::Initialing)
		{
			context->TryCancel();
		}
	}
	bIsShutdowning = true;
}

FString FGrpcResult::GetCodeString() const
{
	static const int preFixLen = FString(TEXT("EGrpcResultCode::")).Len();
	return UEnum::GetValueAsString(Code).RightChop(preFixLen);
}

FString FGrpcResult::GetMessageString() const
{
	return FString::Printf(TEXT("%s:%s"), *GetCodeString(), *Message);
}