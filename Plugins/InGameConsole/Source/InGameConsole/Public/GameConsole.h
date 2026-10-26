// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AutoCompletionProvider.h"
#include "ConsoleCommandBase.h"
#include "ConsoleWidget.h"
#include "Parser.h"
#include "Execution.h"
#include "InputCoreTypes.h"
#include "Net/OnlineEngineInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameConsole.generated.h"

UCLASS(BlueprintType, Config=Game, defaultconfig, meta=(DisplayName = "Game Console"))
class UGameConsoleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config)
	FKey ToggleKey;
};

//~ autocompletion
enum EAutocompletionRequestType
{
	Invalid,
	CommandName,
	ParameterName,
	ParameterValue
};

struct FAutocompletionRequest
{
	EAutocompletionRequestType GetType() const
	{
		return Type;
	}

	FString GetValue() const
	{
		return Value;
	}

	bool IsValid() const
	{
		return Type != Invalid;
	}

	const UParameter* GetParameter() const
	{
		return AutocompleteParameter;
	}

	const UConsoleCommandBase* GetCommand() const
	{
		return Command;
	}

	static FAutocompletionRequest CommandName(const FString& Value)
	{
		return FAutocompletionRequest(EAutocompletionRequestType::CommandName, Value, nullptr, nullptr);
	}

	static FAutocompletionRequest ParamName(const FString& Value, const UConsoleCommandBase* Command)
	{
		return FAutocompletionRequest(ParameterName, Value, Command, nullptr);
	}

	static FAutocompletionRequest ParamValue(const FString& Value, const UConsoleCommandBase* Command, const UParameter* Param)
	{
		return FAutocompletionRequest(ParameterValue, Value, Command, Param);
	}

	static const FAutocompletionRequest INVALID;
	
private:
	FAutocompletionRequest(EAutocompletionRequestType Type,
	                       const FString& InValue, const UConsoleCommandBase* Command, const UParameter* Parameter) :
		Type(Type), Value(InValue), AutocompleteParameter(Parameter), Command(Command)
	{
	}
	
	EAutocompletionRequestType Type;
	FString Value;
	const UParameter* AutocompleteParameter;
	const UConsoleCommandBase* Command;
};

//

/**
 * game console is represented by a game instance subsystem to ensure the following:
 * - command execution history, and console's stdout/stderr persist through the game instance
 * - keeps track of registered console commands over the game instance duration
 * - is the authority that understands the syntax of and parses commands and provides auto-completion
 * - manages the console widget
 *
 * Note: command execution is handled by a world subsystem
 */
UCLASS(BlueprintType)
class INGAMECONSOLE_API UGameConsole : public UGameInstanceSubsystem, public IAutoCompletionProvider
{
	GENERATED_BODY()

	friend class UGameConsoleCommandExecutor;

	UPROPERTY()
	TObjectPtr<USimpleTextPipe> StdOut;

	UPROPERTY()
	TObjectPtr<USimpleTextPipe> StdErr;

	FTokenizer Tokenizer;
	FCLIParser Parser;

	// game console's command executor (for commands with lifetime duration == Game)
	UPROPERTY()
	UCommandExecutor* CommandExecutor;

	/**
	 * an input component that binds an input to opening the console during the game
	 */
	UPROPERTY()
	UInputComponent* InputComponent = nullptr;

	/**
	 * the widget class for the console widget to create
	 */
	UPROPERTY()
	TSubclassOf<UConsoleWidget> WidgetClass;
	
	/**
	 * the console widget, owned and managed by the console
	 */
	UPROPERTY()
	UConsoleWidget* Widget = nullptr;

	// a set of all commands known to this console
	UPROPERTY()
	TSet<TSubclassOf<UConsoleCommandBase>> Commands;

	// history of executed commands
	TArray<FString> ExecutionHistory;

	TWeakObjectPtr<ULatentConsoleCommand> RunningInForeground; // TODO need to set and free this correspondingly

	/**
	 * 
	 * @param CommandName name of the command
	 * @param ContextRelevant return only if the command is relative in current context
	 * @return 
	 */
	UConsoleCommandBase* FindCommandTemplate(const FName& CommandName, bool ContextRelevant = true) const;

	void InitializeInputComponent();

	void InitializeWidget();

	void OnForegroundCommandFinished();
	
	void ProcessConsoleCommand(const APlayerController* Requester, const FString& Command);

	/**
	 * finds an appropriate executor for the command
	 * @param CommandClass command class to find an executor for
	 * @return 
	 */
	UCommandExecutor* FindExecutor(const APlayerController* Requester, const TSubclassOf<UConsoleCommandBase>& CommandClass) const;

public:
	
	UGameConsole()
	{
		StdOut = CreateDefaultSubobject<USimpleTextPipe>(FName(TEXT("StandardOutput")));
		StdErr = CreateDefaultSubobject<USimpleTextPipe>(FName(TEXT("StandardErrorOutput")));
	}

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void ShowUI();

	/**
	 * returns an input component that can open this console 
	 * @return 
	 */
	UInputComponent* GetInputComponent() const
	{
		return InputComponent;
	}

	UFUNCTION(BlueprintCallable)
	void SetWidgetClass(TSubclassOf<UConsoleWidget> InWidgetClass)
	{
		this->WidgetClass = InWidgetClass;
	}
	
	/**
	 * send a line of text to console and ask to process it
	 *
	 * this can result in command execution, communication with the currently running foreground command, or an error
	 * @param Input 
	 */
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
	bool LookUpExecutionHistoryAtOffset(const int32 Offset, FString& HistoricalCommand) const;

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

	USimpleTextPipe* GetMutableStdOut() const
	{
		return StdOut;
	}

	USimpleTextPipe* GetMutableStdErr() const
	{
		return StdErr;
	}

	bool ParseCommand(const FString& Command, UConsoleCommandBase*& CommandTemplate, FCommandArguments& OutArgs, FString& OutErrorMessage);

	FAutocompletionRequest ResolveAutocompletionRequest(const FString& AutocompletionString) const;
	
	/**
	 * returns autocompletions for a piece of text.
	 * 
	 * this function assumes the caret is at the very end of the string. for caret-aware method, see SuggestAutocompletions
	 *
	 * it supports autocompletion of:
	 * - command names
	 * - parameter values (for string params whose values are restricted to a set)
	 * - parameter names
	 * @param InString 
	 * @param AutoCompletions 
	 * @return 
	 */
	virtual bool
	RequestAutocompletions_Implementation(const FString& InString, TSet<FString>& AutoCompletions) const override;

	/**
	 * suggest auto-completions for a command and a caret position
	 * @param InCmdText 
	 * @param CaretPosition caret position is conventionally defined as an index in text where next write will happen
	 * @param AutoCompletions 
	 * @return 
	 */
	UFUNCTION(BlueprintCallable)
	bool SuggestAutoCompletions(const FString& InCmdText, const int32 CaretPosition,
	                            TSet<FString>& AutoCompletions) const;
};

UCLASS()
class UCommandExecutionWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	UPROPERTY()
	UCommandExecutor* Executor;
	
public:
	UCommandExecutionWorldSubsystem()
	{
		Executor = CreateDefaultSubobject<UCommandExecutor>("Executor");
		Executor->bAllowsParallelExecution = false;
		Executor->bCanExecuteLatentCommands = true;
	}
	
	UCommandExecutor* GetExecutor() const
	{
		return Executor;
	}
	
	virtual void Deinitialize() override;
};

UCLASS()
class UGameConsoleInputSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
};
