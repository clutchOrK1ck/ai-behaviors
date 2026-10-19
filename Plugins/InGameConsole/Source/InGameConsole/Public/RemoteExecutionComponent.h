// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ConsoleCommand.h"
#include "Execution.h"
#include "Components/ActorComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "RemoteExecutionComponent.generated.h"

UENUM(BlueprintType)
enum class EExecutionAuthorityType : uint8
{
	All						UMETA(ToolTip = "Clients can execute any non-latent server command"),
	AllInCategory			UMETA(ToolTip = "Clients may execute non-latent server commands in a specific category"),
	SingleCommand			UMETA(ToolTip = "Clients may execute the non-latent server command specified by name")
};

/**
 * authority to execute any or one particular command
 */
struct FExecutionAuthority
{
	EExecutionAuthorityType Type;
	FString Key; // this can mean different things depending on the authority type
};

/**
 * execution authority request, sent by client to server
 */
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

/**
 * an actor component that requests remote execution rights and executes commands on the server
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), NotBlueprintable)
class INGAMECONSOLE_API URemoteExecutionComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class URemoteCommandExecutor;

	friend class URemoteExecutionSubsystem;

	// a replicated array of execution authority items
	TArray<FExecutionAuthority> ExecutionAuthorityList;

	UPROPERTY()
	USimpleTextPipe* StdOut;

	UPROPERTY()
	USimpleTextPipe* StdErr;

	/**
	 * we don't send commands executed by clients to server game console's executor - instead, execute them isolated in the owning controller
	 */
	UPROPERTY()
	URemoteCommandExecutor* RemoteCommandExecutor;

public:
	// Sets default values for this component's properties
	URemoteExecutionComponent();
	
	bool IsPlayerAllowedToExecute(const UConsoleCommand* Command) const;
	
	UFUNCTION(Server, Reliable)
	void ExecuteCommandOnServer(TSubclassOf<UConsoleCommand> CommandClass, const FCommandArguments& Args);

	UFUNCTION(Client, Reliable)
	void ReceiveCommandFeedbackOnClient(const FString& Message, bool bIsStdErr);
	
	/**
	 * ask server for authority to execute all commands/particular command/commands from particular category
	 */
	UFUNCTION(Server, Reliable)
	void RequestServerExecutionAuthority(EExecutionAuthorityType Type, const FString& Key);
};

UCLASS()
class URemoteCommandExecutor : public UCommandExecutor
{
	GENERATED_BODY()

	URemoteExecutionComponent* RemoteExecutionComponent;

protected:
	virtual bool FindOutputChannelsForCommand(const UConsoleCommand* ConsoleCommand, bool bIsExecutingInForeground,
		ITextPipe*& OutStdOut, ITextPipe*& OutStdErr) override;
	
public:
	virtual bool AllowsMultipleProcesses() const override;
	
	void Initialize(URemoteExecutionComponent* InRemoteExecutionComponent);
};

UENUM(BlueprintType)
enum class EProcessRequestAction : uint8
{
	Allow,
	Deny
};

/**
 * a server-only subsystem for managing remote execution authority requests
 * 
 * networking does not exist outside a game world, so a remote execution subsystem must be a level subsystem
 *
 * this also saves us from the necessity to clear stale execution requests and controller pointers on level reloads
 */
UCLASS(BlueprintType)
class URemoteExecutionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	TQueue<FExecutionAuthorityRequest> PendingRequests;

public:

	// this subsystem only exists on the server in networked games
	// TODO authority changes on travel?..
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	void AddRemoteExecutionAuthorityRequest(const FExecutionAuthorityRequest& Request);

	UFUNCTION(BlueprintCallable, meta=(
		ToolTip = "Grant execution authority to the player. You can call this from game mode to grant default server execution authorities to players and avoid exchanging requests"))
	void GrantAuthority(const APlayerController* Player, EExecutionAuthorityType Type, FString Key);

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
