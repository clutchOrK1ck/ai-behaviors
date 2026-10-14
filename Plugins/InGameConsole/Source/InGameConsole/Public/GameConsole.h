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

	UConsoleCommand* FindCommandTemplate(FName CommandName) const
	{
		return KnownCommands[0].GetDefaultObject(); // TODO
	}

public:

	UPROPERTY() // TODO should we expose it somewhere on an engine level? but then it is a hassle if users have to manually specify the existing commands no?
	TArray<TSubclassOf<UConsoleCommand>> KnownCommands;
	
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
	
	void GetKnownCommandNames(TArray<FString>& OutCommandNames) const;
	
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
