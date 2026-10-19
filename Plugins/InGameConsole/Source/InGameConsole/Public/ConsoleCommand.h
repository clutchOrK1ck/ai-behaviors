// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CommandParameters.h"
#include "Commons.h"
#include "ConsoleCommand.generated.h"

struct FCommandExecutionContext
{
	const APlayerController* Caller;		// the player that requested command execution
	FCommandArguments Arguments;
	ITextPipe* StdOut;
	ITextPipe* StdErr;
};

UENUM(BlueprintType)
enum class EReadUserInputResult : uint8
{
	Completed,
	Aborted
};

class FLatentAwaitUserInputAction : public FPendingLatentAction
{
public:
	FLatentAwaitUserInputAction(
		EReadUserInputResult& ResultHandle,
		FLatentActionInfo& Info) :
		Result(ResultHandle),
		ExecutionFunction(Info.ExecutionFunction),
		OutputLink(Info.Linkage),
		CallbackTarget(Info.CallbackTarget)
	{
	}
	
	void AcceptUserInput(const FString& Input)
	{
		Result = EReadUserInputResult::Completed;
		bUserRepliedOrAborted = true;
	}
	
	void Abort()
	{
		Result = EReadUserInputResult::Aborted;
		bUserRepliedOrAborted = true;
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		Response.FinishAndTriggerIf(bUserRepliedOrAborted, ExecutionFunction, OutputLink, CallbackTarget);
	}

private:
	EReadUserInputResult& Result;
	bool bUserRepliedOrAborted = false;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
};

UENUM(BlueprintType)
enum class ECommandLifecycleType : uint8
{
	RecreateEachInvocation UMETA(ToolTip = "The command object will be created new for each invocation. Use this for commands which do not need a persistent state"),
	Persistent UMETA(ToolTip = "The command object will be created once on first invocation and persist through the game instance lifetim. Use this for commands that have their own persistent state")
};

/**
 * a command executable from the game console. Console commands are async by default. They cannot tick but they can
 * execute latent procedures, wait for other objects to finish execution etc. To finish execution, its 'Finish' method
 * must be called
 *
 * Console command instances should be created per invocation
 */
UCLASS(Blueprintable, meta=(ShowWorldContextPin))
class INGAMECONSOLE_API UConsoleCommand : public UObject
{
	GENERATED_BODY()

	DECLARE_DELEGATE(FOnAbortCommand);
	FOnAbortCommand OnCommandAborted;

	DECLARE_DELEGATE(FOnExecutionFinished);
	
	FCommandExecutionContext CurrentExecutionContext;

	/**
	 * this command's input channel
	 */
	UPROPERTY()
	TObjectPtr<UCommandUserInput> UserInput;
	
	// if user needs to supply input for this command to resume
	bool bIsWaitingForUserInput;

	bool bHasFinished; // whether this command has finished execution
	int32 ExitCode; // command exit code, if it finished

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(
		ToolTip = "The command will become callable from console under this name"))
	FName CommandName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(
		ToolTip = "Command category, separate subcategories with a pipe"))
	FString Category;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECommandLifecycleType LifecycleType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(
		ToolTip = "Whether this command is supposed to be executed on the server"))
	bool bIsServerCommand;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, meta=(
		ToolTip = "Parameters for which a value is expected at a given position, rather than supplied by name. Note: in blueprint, you still access positional parameters by name. Being positional only affects how CLI command is parsed"))
	TArray<TObjectPtr<UParameter>> PositionalParameters;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, meta=(ShowOnlyInnerProperties))
	TArray<TObjectPtr<UParameter>> NamedParameters;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSet<FFlagDescription> Flags;

	FOnExecutionFinished OnExecutionFinished;
	
	UConsoleCommand()
	{
		// get a copy of the input channel for the user input
		UserInput = CreateDefaultSubobject<UCommandUserInput>(FName(TEXT("UserInput")));
	}
	
	UFUNCTION(BlueprintPure)
	bool GetIntArgument(FName ArgumentName, int32& Value) const
	{
		return CurrentExecutionContext.Arguments.Get<int32>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetStringArgument(FName ArgumentName, FString& Value) const
	{
		return CurrentExecutionContext.Arguments.Get<FString>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetBoolArgument(FName ArgumentName, bool& Value) const
	{
		return CurrentExecutionContext.Arguments.Get<bool>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetRealArgument(FName ArgumentName, double& Value) const
	{
		return CurrentExecutionContext.Arguments.Get<double>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetVectorArgument(FName ArgumentName, FVector& Value) const
	{
		return CurrentExecutionContext.Arguments.Get<FVector>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetListArgument(FName ArgumentName, TArray<FString>& Value) const
	{
		return CurrentExecutionContext.Arguments.Get<TArray<FString>>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool HasFlag(const FString& Flag) const
	{
		return CurrentExecutionContext.Arguments.HasFlag(Flag);
	}

	UFUNCTION(BlueprintPure)
	const APlayerController* GetCaller() const
	{
		return CurrentExecutionContext.Caller;
	}

	UFUNCTION(BlueprintCallable)
	void WriteMessageToStdErr(const FString& Message) const
	{
		CurrentExecutionContext.StdErr->Write(Message);
	}

	UFUNCTION(BlueprintCallable)
	void WriteMessageToStdOut(const FString& Message) const
	{
		CurrentExecutionContext.StdOut->Write(Message);
	}

	// pause execution, print the prompt to stdout and wait for the user answer
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", ExpandEnumAsExecs = "Result"))
	void RequireUserInput(const UObject* WorldContextObject, const FString& Prompt, FLatentActionInfo LatentInfo, FString& OutUserInput, EReadUserInputResult& Result);

	UFUNCTION(BlueprintCallable)
	void Finish(const int32 InExitCode);
	
	/**
	 * try to push user input, this will do nothing if the command is not waiting for user input
	 * @param Input 
	 */
	bool PushUserInput(const FString& Input)
	{
		if (bIsWaitingForUserInput)
		{
			UserInput->Write(Input);
			return true;
		}

		return false;
	}

	/**
	 * aborts execution of this command (for latent commands)
	 */
	void Abort()
	{
		bIsWaitingForUserInput = false;
		OnCommandAborted.ExecuteIfBound();
		UserInput->OnNewInput.Clear();
		// TODO other. do we need to clear state or even destroy? it largely depends on whether commands should be instantiated or kept alive indefinitely and periodically clean their state
	}

	bool IsCurrentlyExecuting() const
	{
		return !bHasFinished;
	}

	bool HasFinishedExecution() const
	{
		return bHasFinished;
	}
	
	// whether this console command is currently waiting for user input
	bool IsWaitingForUserInput() const;

	bool IsPersistent() const
	{
		return LifecycleType == ECommandLifecycleType::Persistent;
	}
	
	const UParameter* FindNamedParamByLongName(const FName& Name);
	const UParameter* FindNamedParamByShortName(const FName& Name);
	
protected:
	/**
	 * implement command execution logic here
	 *
	 * note that this function takes no arguments, access them with the corresponding class methods instead
	 * @return integer return code
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void ExecuteImpl();
	
public:
	int32 ExecuteWithContext(const FCommandExecutionContext& Context)
	{
		CurrentExecutionContext = Context;
		ExecuteImpl();
		return 0; // TODO
	}
	
	UFUNCTION(BlueprintImplementableEvent, meta=(
		ToolTip = "Override this to provide parameter allowed values dynamically"))
	void GetAllowedValues(const FName& ParamName, TSet<FString>& OutValues) const;

	UFUNCTION(BlueprintImplementableEvent, meta=(
		ToolTip = "Whether this command makes sense in current context. The logic can depend on the loaded level, game mode, presence of certain actors, player controller class etc. etc."))
	bool IsRelevantInCurrentContext() const;

	/**
	 * a way for the command to check if a parameter value is in the allowed set
	 *
	 * use this for parameter values for which allowed values are resolved dynamically by the command itself
	 *
	 * NOTE: this does NOT check if the value is in static set defined in the parameter, this ONLY checks against
	 * the dynamic set of allowed values returned by the command itself
	 * @param LongParamName 
	 * @param Value 
	 * @return 
	 */
	bool CheckParamValueIsInAllowedSet(const FName& LongParamName, const FString& Value) const;
};

/**
 * a set of commands.
 * The console then registers one or more command sets on game start, level loads etc.
 */
UCLASS(Blueprintable, BlueprintType)
class UConsoleCommandSet : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSet<TSubclassOf<UConsoleCommand>> Commands;
};