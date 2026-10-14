// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ConsoleCommand.h"
#include "Execution.generated.h"

UCLASS(Abstract)
class UCommandExecutor : public UObject
{
	GENERATED_BODY()

	/**
	 * keeps references to persistent commands (commands that are instantiated once on the first invocation and kept alive for the entire game instance lifetime)
	 */
	UPROPERTY()
	TArray<UConsoleCommand*> PersistentCommands;
	
	UPROPERTY()
	TArray<UConsoleCommand*> RunningInBackground;

	UPROPERTY()
	UConsoleCommand* RunningInForeground = nullptr;
	
protected:
	
	virtual bool FindOutputChannelsForBackgroundCommand(const UConsoleCommand* ConsoleCommand,
		bool bIsExecutingInForeground,
		ITextPipe*& OutStdOut,
		ITextPipe*& OutStdErr)
	{
		return false;
	}

	void HandleExecutionFinished(UConsoleCommand* Command, bool bForeground);

	bool ExecuteInternal(const TSubclassOf<UConsoleCommand>& CommandClass, bool bForeground, const APlayerController* Requester, const FCommandArguments& Args);

public:
	
	bool ExecuteInBackground(const APlayerController* Requester, const TSubclassOf<UConsoleCommand>& CommandClass, const FCommandArguments& Args);
	bool ExecuteInForeground(const APlayerController* Requester, const TSubclassOf<UConsoleCommand>& CommandClass, const FCommandArguments& Args);
	
	// whether this executor allows multiple commands running at the same time
	virtual bool AllowsMultipleProcesses() const
	{
		return true;
	}

	/**
	 * a command executing 'in foreground' occupies this executor's stdout and stderr
	 * @return
	 */
	virtual UConsoleCommand* GetCurrentForegroundCommand() const
	{
		return RunningInForeground;
	}

	/**
	 * an array of currently executing commands
	 * @return 
	 */
	virtual TArray<UConsoleCommand*> GetRunningCommands() const;
};
