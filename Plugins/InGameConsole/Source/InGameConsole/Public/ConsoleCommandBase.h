// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CommandParameters.h"
#include "Commons.h"
#include "ConsoleCommandBase.generated.h"

/**
 * the command execution context contains:
 * - a player controller who requested this command execution
 * - command arguments supplied for this command invocation
 * - StdOut and StdErr where this command can write to
 *
 * note that StdOut and StdErr should be assigned by the command executor.
 * A command would *usually* inherit some external StdOut/StdErr, for example those of the console,
 * instead of writing to its own StdOut/StdErr
 */
struct FCommandExecutionContext
{
	TWeakObjectPtr<const APlayerController> Caller;		// the player that requested command execution, be careful because this may become invalid in a lot of edge cases (player disconnected, player controller destroyed before level finished play etc etc)
	TWeakObjectPtr<UWorld> World;										// pointer to the world in which the command executes. this is reliable for non-persistent and world commands but absolutely unreliable for game-instance commands
	FCommandArguments Arguments;
	ITextPipe* StdOut;
	ITextPipe* StdErr;
};

/**
 * a latent action waiting for user reply. finishes if AcceptUserInput is called
 */
class FLatentAwaitUserInputAction : public FPendingLatentAction
{
public:
	FLatentAwaitUserInputAction(FString& OutUserInput, FLatentActionInfo& Info) :
		OutUserInput(OutUserInput),
		ExecutionFunction(Info.ExecutionFunction),
		OutputLink(Info.Linkage),
		CallbackTarget(Info.CallbackTarget)
	{
	}
	
	void AcceptUserInput(const FString& Input)
	{
		OutUserInput = Input;
		bUserReplied = true;
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		Response.FinishAndTriggerIf(bUserReplied, ExecutionFunction, OutputLink, CallbackTarget);
	}

private:
	FString& OutUserInput;
	bool bUserReplied = false;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
};

UENUM(BlueprintType)
enum class ECommandLifeTime : uint8
{
	Invocation		UMETA(ToolTip = "A command instance is recreated for each invocation and destroyed when execution finishes. For latent commands, an instance is destroyed when the earlier of the two happens: execution finishes or level is destroyed"),
	Level			UMETA(ToolTip = "A command instance is created on first invocation and persists for the current level (UWorld) duration"),
	Game			UMETA(ToolTip = "A command instance is created on first invocation and persists for the duration of the game instance")
};

UENUM(BlueprintType)
enum class EParameterValueType : uint8
{
	Integer,
	Real,
	String,
	Vector,
	List,
	Boolean
};

USTRUCT(BlueprintType)
struct FCommandParameterDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName Name;

	UPROPERTY(BlueprintReadOnly)
	FName Alias;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	bool bRequired;
	
	UPROPERTY(BlueprintReadOnly)
	EParameterValueType ValueType = EParameterValueType::Boolean;

	UPROPERTY(BlueprintReadOnly)
	FString DefaultValue;

	UPROPERTY(BlueprintReadOnly, meta=(
		ToolTip = "A piece of text describing how the parameter value is validated"))
	FString ValidationInfo;
};

/**
 * a blueprint-facing command definition, performs solely informative role
 */
USTRUCT(BlueprintType)
struct FCommandDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName Name = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FString Category;
	
	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	bool bLatent = false;

	UPROPERTY(BlueprintReadOnly)
	bool bServer = false;

	UPROPERTY(BlueprintReadOnly)
	ECommandLifeTime LifecycleType = ECommandLifeTime::Invocation;

	UPROPERTY(BlueprintReadOnly)
	TArray<FCommandParameterDefinition> PositionalParameters;

	UPROPERTY(BlueprintReadOnly)
	TArray<FCommandParameterDefinition> NamedParameters;

	UPROPERTY(BlueprintReadOnly)
	TArray<FFlagDescription> Flags;

	bool IsFilled() const
	{
		return Name != NAME_None;
	}
};

/**
 * a command executable from the game console. Console commands are async by default. They cannot tick but they can
 * execute latent procedures, wait for other objects to finish execution etc. To finish execution, its 'Finish' method
 * must be called
 *
 * Console command instances should be created per invocation
 */
UCLASS(NotBlueprintable)
class INGAMECONSOLE_API UConsoleCommandBase : public UObject
{
	GENERATED_BODY()

	FCommandDefinition CommandDefinition;

protected:
	int32 ExitCode = -1; // command exit code
	FCommandExecutionContext CurrentExecutionContext;
	
	FCommandParameterDefinition MakeParameterDefinition(const UParameter* Parameter) const;
	void FillParameterAndFlagDefinitions(FCommandDefinition& OutCommandDefinition) const;
	
	virtual void GenerateCommandDefinition(FCommandDefinition& OutCommandDefinition) const;
	
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(
		ToolTip = "The command will become callable from console under this name"))
	FName CommandName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(
		ToolTip = "Command category, separate subcategories with a pipe"))
	FString Category;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(MultiLine=true))
	FString Description;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECommandLifeTime LifecycleType;

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

	virtual UWorld* GetWorld() const override;
	
	template<typename ValueType, typename ParamType>
	bool TryGetArgumentOrDefaultValue(FName ArgumentName, ValueType& Value) const
	{
		if (CurrentExecutionContext.Arguments.HasArgumentWithName(ArgumentName))
		{
			return CurrentExecutionContext.Arguments.Get<ValueType>(ArgumentName, Value);
		}

		if (const TConsoleParameter<ValueType>* ParamSpec = Cast<ParamType>(FindParameter(ArgumentName)))
		{
			if (ParamSpec->IsRequired())
			{
				return false;
			}

			Value = ParamSpec->GetDefaultValue();
			return true;
		}

		return false;
	}
	
	UFUNCTION(BlueprintPure)
	bool GetIntArgument(FName ArgumentName, int32& Value) const
	{
		return TryGetArgumentOrDefaultValue<int32, UIntegerParameter>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetStringArgument(FName ArgumentName, FString& Value) const
	{
		return TryGetArgumentOrDefaultValue<FString, UStringParameter>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetBoolArgument(FName ArgumentName, bool& Value) const
	{
		return TryGetArgumentOrDefaultValue<bool, UBoolParameter>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetRealArgument(FName ArgumentName, double& Value) const
	{
		return TryGetArgumentOrDefaultValue<double, URealParameter>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetVectorArgument(FName ArgumentName, FVector& Value) const
	{
		return TryGetArgumentOrDefaultValue<FVector, UVectorParameter>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool GetListArgument(FName ArgumentName, TArray<FString>& Value) const
	{
		return TryGetArgumentOrDefaultValue<TArray<FString>, UListParameter>(ArgumentName, Value);
	}

	UFUNCTION(BlueprintPure)
	bool HasFlag(const FString& Flag) const
	{
		return CurrentExecutionContext.Arguments.HasFlag(Flag);
	}

	UFUNCTION(BlueprintPure)
	const APlayerController* GetCaller() const
	{
		return CurrentExecutionContext.Caller.Get();
	}

	UFUNCTION(BlueprintPure)
	bool IsOptionalArgumentPresent(FName ArgName) const
	{
		return CurrentExecutionContext.Arguments.HasArgumentWithName(ArgName);
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

	UFUNCTION(BlueprintPure, meta=(
		ToolTip = "True if the command is running on dedicated or listen server, or in a standalone game"))
	bool ExecutingOnServer() const
	{
		return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
	}

	// persistent commands exist for the game instance's duration
	bool IsPersistent() const
	{
		return LifecycleType == ECommandLifeTime::Level || LifecycleType == ECommandLifeTime::Game;
	}
	
	const UParameter* FindNamedParamByLongName(const FName& Name) const;
	const UParameter* FindNamedParamByShortName(const FName& Name) const;

	/**
	 * find a parameter by its canonical name
	 * @param Name parameter's canonical name (not its alias)
	 * @return 
	 */
	const UParameter* FindParameter(const FName& Name) const;
	
	const FCommandDefinition& GetCommandDefinition();
	
	virtual void ExecuteWithContext(const FCommandExecutionContext& Context) PURE_VIRTUAL(UConsoleCommandBase::ExecuteWithContext,)

	/**
	 * checks whether all required arguments have been supplied and that there are no unknown arguments
	 * @param InArgs 
	 * @param OutError 
	 * @return 
	 */
	bool ValidateArguments(const FCommandArguments& InArgs, FString& OutError) const;
	
	void GetParameterAndFlagNames(TArray<FName>& Names) const;
	
	UFUNCTION(BlueprintImplementableEvent, meta=(
		ToolTip = "Override this to provide parameter allowed values dynamically"))
	void GetAllowedValues(const FName& ParamName, TSet<FString>& OutValues) const;

	UFUNCTION(BlueprintNativeEvent, meta=(
		ToolTip = "Whether this command makes sense in current context. The logic can depend on the loaded level, game mode, presence of certain actors, player controller class etc. etc."))
	bool IsRelevantInCurrentContext() const;

	bool IsRelevantInCurrentContext_Implementation() const;
	
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

	int32 GetExitCode() const
	{
		return ExitCode;
	}
};

UENUM(BlueprintType)
enum class EOnLatentCommandExecutionEndingReason : uint8
{
	ExecutionFinished,
	ExecutionAborted
};

UCLASS(Blueprintable, meta=(ToolTip = "A latent console command can execute over multiple frames and must exist finish execution with its 'Finish' method"))
class INGAMECONSOLE_API ULatentConsoleCommand : public UConsoleCommandBase
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE(FOnAbortCommand);
	DECLARE_MULTICAST_DELEGATE(FOnExecutionFinished);

	/**
	 * this command's input channel
	 */
	UPROPERTY()
	TObjectPtr<USimpleTextPipe> UserInput;
	
	// if user needs to supply input for this command to resume, this is set to true by calling 'RequireUserInput'
	bool bIsWaitingForUserInput;

	bool bIsExecuting; // whether this command is currently executing

	void FinishExecutionAndCleanUp(EOnLatentCommandExecutionEndingReason Reason);
	
protected:
	/**
	 * implement command execution logic here
	 *
	 * note that this function takes no arguments, access them with the corresponding class methods instead
	 * @return integer return code
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void ExecuteImpl();

	virtual void GenerateCommandDefinition(FCommandDefinition& OutCommandDefinition) const override;

public:
	FOnExecutionFinished OnExecutionFinished;
	FOnAbortCommand OnCommandAborted;
	
	ULatentConsoleCommand()
	{
		UserInput = CreateDefaultSubobject<USimpleTextPipe>(FName(TEXT("UserInput")));
	}
	
	virtual void ExecuteWithContext(const FCommandExecutionContext& Context) override;

	// pause execution, print the prompt to stdout and wait for the user answer
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject"))
	void RequireUserInput(const UObject* WorldContextObject, const FString& Prompt, FLatentActionInfo LatentInfo, FString& OutUserInput);

	UFUNCTION(BlueprintCallable)
	void Finish(const int32 InExitCode);
	
	UFUNCTION(BlueprintImplementableEvent, meta=(
		ToolTip = "This is the place to perform clean-up of any nature when the command has finished or been aborted"))
	void OnExecutionEnding(EOnLatentCommandExecutionEndingReason Reason);
	
	/**
	 * try to push user input, this will do nothing if the command is not waiting for user input
	 * @param Input 
	 */
	bool PushUserInput(const FString& Input);

	/**
	 * stops command execution violently
	 */
	void Abort();

	bool IsCurrentlyExecuting() const
	{
		return bIsExecuting;
	}

	bool HasFinishedExecution() const
	{
		return !bIsExecuting;
	}
	
	// whether this console command is currently waiting for user input
	bool IsWaitingForUserInput() const
	{
		return bIsWaitingForUserInput;
	}
};

UCLASS(Blueprintable, meta=(ToolTip = "A console command that finishes execution its logic within a single frame"))
class INGAMECONSOLE_API UConsoleCommand : public UConsoleCommandBase
{
	GENERATED_BODY()

protected:

	UFUNCTION(BlueprintImplementableEvent, meta=(ForceAsFunction,
		ToolTip = "Command execution logic. Should return an integer program return code"))
	int32 ExecuteImpl();

public:
	virtual void ExecuteWithContext(const FCommandExecutionContext& Context) override
	{
		CurrentExecutionContext = Context;
		ExitCode = ExecuteImpl();
	}
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
	TSet<TSubclassOf<UConsoleCommandBase>> Commands;
};