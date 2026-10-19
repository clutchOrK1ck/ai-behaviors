// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ConsoleCommand.h"
#include "Components/ActorComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "RemoteExecutionComponent.generated.h"

UENUM(BlueprintType)
enum class EExecutionAuthorityType : uint8
{
	All,
	AllInCategory,
	SingleCommand
};

/**
 * authority to execute any or one particular command
 */
struct FExecutionAuthority
{
	EExecutionAuthorityType Type;
	FString Key; // this can mean different things depending on the authority type
};

USTRUCT(BlueprintType)
struct FExecutionAuthorityRequest
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	const APlayerController* Requester;

	UPROPERTY(BlueprintReadOnly)
	EExecutionAuthorityType Type;

	UPROPERTY(BlueprintReadOnly)
	FString Key;
};

struct FExecutionAuthorityRegister
{
private:
	TArray<FExecutionAuthority> ExecutionAuthorityList;

public:
	bool IsPlayerAllowedToExecute(const APlayerController* Player, const UConsoleCommand* Command) const;
};

UCLASS(Abstract)
class URemoteExecutionRequest : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly)
	APlayerController* RequestingPlayer;
};

UCLASS(BlueprintType)
class URemoteExecutionAuthorityRequest : public URemoteExecutionRequest
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class URemoteCommandExecutionRequest : public URemoteExecutionRequest
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly)
	FString Command;
};

/**
 * an actor component that allows clients send execution requests to server for server-only console commands
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INGAMECONSOLE_API URemoteExecutionComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class URemoteExecutionSubsystem;

	// a replicated array of execution authority items
	TArray<FExecutionAuthority> ExecutionAuthorityList;

public:
	// Sets default values for this component's properties
	URemoteExecutionComponent();
	
	bool IsPlayerAllowedToExecute(const UConsoleCommand* Command) const;

	void ExecuteServerOnlyCommand(const FString& Command) const;
	
	/**
	 * attempts to execute a console command on the server, this will fail if authority is insufficient
	 * @param Command 
	 */
	UFUNCTION(Server, Reliable)
	void ExecuteCommandOnServer(const FString& Command);

	/**
	 * ask server for authority to execute all commands/particular command/commands from particular category
	 */
	UFUNCTION(Server, Reliable)
	void RequestServerExecutionAuthority(EExecutionAuthorityType Type, const FString& Key);
};

UENUM(BlueprintType)
enum class EProcessRequestAction : uint8
{
	Allow,
	Deny
};

/**
 * networking does not exist outside a game world, so a remote execution subsystem must be a level subsystem
 *
 * this also saves us from the necessity to clear stale execution requests and controller pointers on level reloads
 *
 * this subsystem only exists on the server
 */
UCLASS(BlueprintType)
class URemoteExecutionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	FExecutionAuthorityRegister ExecutionAuthorityRegister;

	TQueue<FExecutionAuthorityRequest> PendingRequests;

public:

	UPROPERTY(BlueprintReadOnly)
	FInstancedStruct TestInstancedStruct;

	// this subsystem only exists on the server in networked games
	// TODO authority changes on travel?..
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	void AddRemoteExecutionAuthorityRequest(const FExecutionAuthorityRequest& Request);
	
	UFUNCTION(BlueprintCallable)
	bool GetOldestRequest(FExecutionAuthorityRequest& Request);

	/**
	 * 
	 * @param Action allow or deny a request
	 */
	UFUNCTION(BlueprintCallable)
	void ProcessOldestRequest(EProcessRequestAction Action);
	
	UFUNCTION(BlueprintCallable)
	void PurgeAllRequests();
};
