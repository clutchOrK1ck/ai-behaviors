// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AutoCompletionProvider.h"
#include "ConsoleCommand.h"
#include "Parser.h"
#include "Execution.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameConsole.generated.h"

UCLASS(BlueprintType)
class INGAMECONSOLE_API UGameConsole : public UGameInstanceSubsystem, public IAutoCompletionProvider
{
	GENERATED_BODY()

	friend class UGameConsoleCommandExecutor;
	
	UPROPERTY()
	TObjectPtr<UConsoleOutputStream> StdOut;

	UPROPERTY()
	TObjectPtr<UConsoleOutputStream> StdErr;

	FTokenizer Tokenizer;
	FCLIParser Parser;

	UPROPERTY()
	UGameConsoleCommandExecutor* CommandExecutor;

	// a set of all commands known to this console
	TSet<TSubclassOf<UConsoleCommand>> Commands;
	
	// history of executed commands
	TArray<FString> ExecutionHistory;

	/**
	 * 
	 * @param CommandName name of the command
	 * @param ContextRelevant return only if the command is relative in current context
	 * @return 
	 */
	UConsoleCommand* FindCommandTemplate(const FName& CommandName, bool ContextRelevant = true) const;

public:
	
	UGameConsole()
	{
		StdOut = CreateDefaultSubobject<UConsoleOutputStream>(FName(TEXT("StandardOutput")));
		StdErr = CreateDefaultSubobject<UConsoleOutputStream>(FName(TEXT("StandardErrorOutput")));
	}

	/**
	 * send a line of text to console and ask to process it
	 *
	 * this can result in command execution, communication with the currently running foreground command, or an error
	 * @param Input 
	 */
	UFUNCTION(BlueprintCallable)
	void ProcessConsoleInput(const FString& Input);

	/**
	 * registers commands from the supplied command set
	 * @param CommandSet 
	 */
	UFUNCTION(BlueprintCallable)
	void AddCommandSet(const UConsoleCommandSet* CommandSet);

	/**
	 * look up the executed command in the execution history
	 *
	 * @param Offset a positive integer for the offset (negative ints will be converted by positive). Offset = 1 means looking at the last executed command
	 * @param HistoricalCommand 
	 * @return true if the historical entry exists
	 */
	UFUNCTION(BlueprintCallable)
	bool LookUpExecutionHistoryAtOffset(const uint32 Offset, FString& HistoricalCommand) const;

	/**
	 * kills all executing commands and resets console state
	 */
	UFUNCTION(BlueprintCallable)
	void Reset();

	/**
	 * returns a list o names of known commands, optionally respecting context relevancy
	 * @param OutCommandNames 
	 * @param bContextRelevant whether to filter for commands relevant for current context
	 */
	void GetKnownCommandNames(TArray<FString>& OutCommandNames, bool bContextRelevant = true) const;
	
	UFUNCTION(BlueprintPure)
	const UConsoleOutputStream* GetStdOut()
	{
		return StdOut;
	}

	UFUNCTION(BlueprintPure)
	const UConsoleOutputStream* GetStdErr()
	{
		return StdErr;
	}

	virtual bool RequestAutocompletions_Implementation(const FString& InString, TSet<FString>& AutoCompletions) const override;
};

UCLASS()
class UGameConsoleCommandExecutor : public UCommandExecutor
{
	GENERATED_BODY()
	
	const UGameConsole* Console = nullptr;
	
public:

	void Initialize(const UGameConsole* InConsole)
	{
		this->Console = InConsole;
	}
	
	virtual bool AllowsMultipleProcesses() const override
	{
		return false;
	}
	
	virtual bool FindOutputChannelsForBackgroundCommand(const UConsoleCommand* ConsoleCommand,
		bool bIsExecutingInForeground, ITextPipe*& OutStdOut, ITextPipe*& OutStdErr) override;
};
