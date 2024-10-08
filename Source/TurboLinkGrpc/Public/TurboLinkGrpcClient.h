// Copyright (C) Developed by Neo Jin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TurboLinkGrpcService.h"
#include "TurboLinkGrpcClient.generated.h"

class UGrpcService;
class GrpcContext;

UENUM(BlueprintType)
enum class EGrpcContextState : uint8
{
	Ready = 0,
	Initialing = 1,
	Busy = 2,
	Done = 3
};

UENUM(BlueprintType)
enum class EGrpcResultCode : uint8
{
	Ok = 0,
	Cancelled = 1,
	Unknown = 2,
	InvalidArgument = 3,
	DeadlineExceeded = 4,
	NotFound = 5,
	AlreadyExists = 6,
	PermissionDenied = 7,
	ResourceExhausted = 8,
	FailedPrecondition = 9,
	Aborted = 10,
	OutOfRange = 11,
	Unimplemented = 12,
	Internal = 13,
	Unavailable = 14,
	DataLoss = 15,
	Unauthenticated = 16,

	ConnectionFailed = 100,

	NotDefined = 0xFF,
};

USTRUCT(BlueprintType)
struct TURBOLINKGRPC_API FGrpcResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = TurboLink)
	EGrpcResultCode Code;

	UPROPERTY(BlueprintReadWrite, Category = TurboLink)
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = TurboLink)
	FString Details;

	FString GetCodeString() const;
	FString GetMessageString() const;

	FGrpcResult() : Code(EGrpcResultCode::NotDefined), Message(TEXT("")), Details(TEXT("")) { }
	FGrpcResult(EGrpcResultCode _Code, const FString& _Message, const FString& _Details) : Code(_Code), Message(_Message), Details(_Details) { }
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnContextHandleStateChange, EGrpcContextState/* NewState*/);
USTRUCT(BlueprintType)
struct FGrpcContextHandle
{
	GENERATED_BODY()

	uint32 Value;

	operator const uint32&() const { return Value; }
	FGrpcContextHandle() { this->Value = 0; }
	FGrpcContextHandle(const uint32& _Value) { this->Value = _Value; }

	FOnContextHandleStateChange OnContextStateChanged;
};

FORCEINLINE uint32 GetTypeHash(const FGrpcContextHandle& GrpcContextHandle)
{
	return GrpcContextHandle.Value;
}

USTRUCT(BlueprintType)
struct TURBOLINKGRPC_API FGrpcMetaData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = TurboLink)
	TMap<FString, FString> MetaData;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContextStateChange, const FGrpcContextHandle&, Handle, EGrpcContextState, NewState);

UCLASS(ClassGroup = TurboLink, BlueprintType, Abstract)
class TURBOLINKGRPC_API UGrpcClient : public UObject
{
	GENERATED_BODY()
	friend class UGrpcService;

public:
	UPROPERTY(BlueprintAssignable, Category = TurboLink)
	FOnContextStateChange OnContextStateChange;

	UFUNCTION(BlueprintCallable, Category = TurboLink)
	EGrpcContextState GetContextState(const FGrpcContextHandle& Handle) const;

	UFUNCTION(BlueprintCallable, Category = TurboLink)
	bool AddMetadataToContext(const FGrpcContextHandle& Handle, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = TurboLink)
	void TryCancelContext(const FGrpcContextHandle& Handle);

	UFUNCTION(BlueprintCallable, Category = TurboLink)
	virtual void Shutdown();

	UGrpcService* GetService() const { return Service; }

protected:
	UGrpcService* Service;
	TMap<uint32, TSharedPtr<GrpcContext>> ContextMap;
	bool bIsShutdowning=false;

	template<typename T>
	TSharedPtr<T> MakeContext(const FGrpcContextHandle& handle)
	{
		auto context = MakeShared<T>(handle, Service, this);
		AddContext(context);
		return context;
	}

	void AddContext(TSharedPtr<GrpcContext> Context);
	TSharedPtr<GrpcContext>* GetContext(const FGrpcContextHandle& Handle);
	void RemoveContext(const FGrpcContextHandle& Handle);

	void Tick(float DeltaTime);

public:
	UGrpcClient();
	virtual ~UGrpcClient();
};