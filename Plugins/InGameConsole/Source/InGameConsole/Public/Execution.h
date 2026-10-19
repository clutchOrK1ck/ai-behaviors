// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ConsoleCommandBase.h"
#include "Execution.generated.h"

UCLASS(Abstract)
class UCommandExecutor : public UObject
{
	GENERATED_BODY()

	/**
	 * keeps references to persistent commands (commands that are instantiated once on the first invocation and kept alive for the entire game instance lifetime)
	 */
	UPROPERTY()
	TArray<UConsoleCommandBase*> PersistentCommands;
	
	UPROPERTY()
	TArray<ULatentConsoleCommand*> RunningInBackground;

	UPROPERTY()
	ULatentConsoleCommand* RunningInForeground = nullptr;
	
protected:
	
	virtual bool FindOutputChannelsForCommand(const UConsoleCommandBase* ConsoleCommand,
		bool bIsExecutingInForeground,
		ITextPipe*& OutStdOut,
		ITextPipe*& OutStdErr)
	{
		return false;
	}

	void HandleExecutionFinished(ULatentConsoleCommand* Command, bool bForeground);

	bool ExecuteInternal(const TSubclassOf<UConsoleCommandBase>& CommandClass, bool bForeground, const APlayerController* Requester, const FCommandArguments& Args);

public:
	
	bool ExecuteInBackground(const APlayerController* Requester, const TSubclassOf<UConsoleCommandBase>& CommandClass, const FCommandArguments& Args);
	bool ExecuteInForeground(const APlayerController* Requester, const TSubclassOf<UConsoleCommandBase>& CommandClass, const FCommandArguments& Args);
	
	// whether this executor allows multiple commands running at the same time
	virtual bool AllowsMultipleProcesses() const
	{
		return true;
	}

	// whether this executor can execute latent commands, i.e. commands executing over multiple frames
	virtual bool CanExecuteLatentCommands() const
	{
		return true;
	}

	/**
	 * a command executing 'in foreground' occupies this executor's stdout and stderr
	 * @return
	 */
	virtual ULatentConsoleCommand* GetCurrentForegroundCommand() const
	{
		return RunningInForeground;
	}

	/**
	 * an array of currently executing commands
	 * @return 
	 */
	virtual TArray<ULatentConsoleCommand*> GetRunningCommands() const;

	/**
	 * kills all commands being executed and clears state
	 */
	void Reset();
};
