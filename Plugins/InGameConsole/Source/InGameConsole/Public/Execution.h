// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ConsoleCommandBase.h"
#include "Execution.generated.h"

UCLASS()
class UCommandExecutor : public UObject
{
	GENERATED_BODY()

	/**
	 * persistent command instances managed by this executor
	 */
	UPROPERTY()
	TArray<UConsoleCommandBase*> PersistentInstances;
	
	UPROPERTY()
	TArray<ULatentConsoleCommand*> RunningLatentCommands;

	// either creates a new instance or gets an existing one (for persistent commands)
	UConsoleCommandBase* GetOrCreateCommandInstance(TSubclassOf<UConsoleCommandBase> CommandClass);
	
protected:

	void HandleExecutionFinished(ULatentConsoleCommand* Command);
	
public:

	bool bAllowsParallelExecution; // whether this executor can be executing multiple (latent) commands in parallel
	bool bCanExecuteLatentCommands; // whether this executor can execute latent commands

	bool Execute(const APlayerController* Requester,
		const TSubclassOf<UConsoleCommandBase>& CommandClass,
		const FCommandArguments& Args,
		ITextPipe* StdOut,
		ITextPipe* StdErr);
	
	bool Execute(const APlayerController* Requester,
		const TSubclassOf<UConsoleCommandBase>& CommandClass,
		const FCommandArguments& Args,
		ITextPipe* StdOut,
		ITextPipe* StdErr,
		ULatentConsoleCommand*& OutCommandHandle);

	/**
	 * an array of currently executing commands
	 * @return 
	 */
	const TArray<ULatentConsoleCommand*>& GetRunningCommands() const
	{
		return RunningLatentCommands;
	}

	/**
	 * kills all commands being executed and clears state
	 */
	void Reset();
};
