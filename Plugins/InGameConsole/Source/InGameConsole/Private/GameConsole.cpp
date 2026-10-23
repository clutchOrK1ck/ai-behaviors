// Fill out your copyright notice in the Description page of Project Settings.

#include "GameConsole.h"
#include "ConsoleCommandBase.h"
#include "RemoteExecutionComponent.h"
#include "Kismet/GameplayStatics.h"

const FAutocompletionRequest FAutocompletionRequest::INVALID = FAutocompletionRequest(Invalid, "", nullptr, nullptr);

void UCommandExecutionWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();

	Executor->Reset();
	Executor->MarkAsGarbage();
}

void UGameConsoleInputSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	if (const UGameConsole* Console = GetWorld()->GetGameInstance()->GetSubsystem<UGameConsole>())
	{
		NewPlayerController->PushInputComponent(Console->GetInputComponent());
	} else
	{
		UE_LOG(LogTemp, Error, TEXT("Error while trying to push console input component: game console subsystem not found"));
	}
}

UConsoleCommandBase* UGameConsole::FindCommandTemplate(const FName& CommandName, bool ContextRelevant) const
{
	for (TSubclassOf<UConsoleCommandBase> CommandCls : Commands)
	{
		UConsoleCommandBase* CDO = CommandCls.GetDefaultObject();
		if (CDO->CommandName.IsEqual(CommandName))
		{
			if (!ContextRelevant || CDO->IsRelevantInCurrentContext())
			{
				return CDO;
			}
		}
	}

	return nullptr;
}

void UGameConsole::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// create the executor
	CommandExecutor = NewObject<UCommandExecutor>(this, UCommandExecutor::StaticClass());
	CommandExecutor->bAllowsParallelExecution = false;
	CommandExecutor->bCanExecuteLatentCommands = true;
	
	InitializeInputComponent();
	InitializeWidget();
}


void UGameConsole::InitializeInputComponent()
{
	if (InputComponent == nullptr)
	{
		InputComponent = NewObject<UInputComponent>(this, UInputComponent::StaticClass());
		
		InputComponent->BindKey(
			GetDefault<UGameConsoleSettings>()->ToggleKey,
			IE_Pressed,
			this,
			&UGameConsole::ShowUI);
	}
}

void UGameConsole::InitializeWidget()
{
	if (WidgetClass.Get() != nullptr)
	{
		Widget = CreateWidget<UConsoleWidget>(this->GetGameInstance(), WidgetClass.Get());

		if (!Widget)
		{
			return;
		}

		// make the widget listen to stdout and stderr of the game console
		this->StdOut->OnNewItem.AddLambda([this](const FString& Line)
		{
			Widget->OnNewLine(Line, EConsoleLineType::Display);
		});

		this->StdErr->OnNewItem.AddLambda([this](const FString& Line)
		{
			Widget->OnNewLine(Line, EConsoleLineType::Error);
		});
	}
}

void UGameConsole::ProcessConsoleCommand(const APlayerController* Requester, const FString& Command)
{
	FString ParsingError;
	FCommandArguments Args;
	UConsoleCommandBase* CommandTemplate;

	if (!ParseCommand(Command, CommandTemplate, Args, ParsingError))
	{
		StdErr->Write(ParsingError); // TODO deal with error accumulation and propagation
		return;
	}

	const bool bRemoteExecution = CommandTemplate->bIsServerCommand && GetWorld()->GetNetMode() == NM_Client;
	
	// check that remote execution component exists on the player controller
	if (bRemoteExecution)
	{
		if (CommandTemplate->IsA<ULatentConsoleCommand>())
		{
			StdErr->Write("Remote execution of latent commands is not allowed");
			return;
		}
		
		URemoteExecutionComponent* RemoteExecutionComponent = Requester->FindComponentByClass<URemoteExecutionComponent>();
		if (!RemoteExecutionComponent)
		{
			StdErr->Write("Your game configuration does not allow remote execution of server-only commands, remote execution component is missing");
			return;
		}

		if (!RemoteExecutionComponent->IsPlayerAllowedToExecute(CommandTemplate))
		{
			StdErr->Write("You are not allowed to execute bla bla, this is server-only command, please request authority");
			return;
		}
		
		RemoteExecutionComponent->ExecuteCommandOnServer(Command);
	} else
	{
		if (UCommandExecutor* Executor = FindExecutor(Requester, CommandTemplate->GetClass()))
		{
			ULatentConsoleCommand* CommandHandle;
			if (!Executor->Execute(Requester, CommandTemplate->GetClass(), Args, StdOut, StdErr, CommandHandle))
			{
				UE_LOG(LogTemp, Error, TEXT("Command execution failed: '%s'"), *Command);
				return;
			}

			if (CommandHandle)
			{
				RunningInForeground = CommandHandle;
			}
		} else
		{
			UE_LOG(LogTemp, Error, TEXT("Could not find command executor: '%s'"), *Command);
		}
	}

	ExecutionHistory.Add(Command);
}

UCommandExecutor* UGameConsole::FindExecutor(const APlayerController* Requester, const TSubclassOf<UConsoleCommandBase>& CommandClass) const
{
	const auto CommandTemplate = CommandClass.GetDefaultObject();

	// invocation and world-persistent commands are delegated to world subsystem for execution
	if (CommandTemplate->LifecycleType == ECommandLifeTime::Invocation || CommandTemplate->LifecycleType == ECommandLifeTime::Level)
	{
		if (Requester->GetWorld() && Requester->GetWorld()->GetSubsystem<UCommandExecutionWorldSubsystem>())
		{
			return Requester->GetWorld()->GetSubsystem<UCommandExecutionWorldSubsystem>()->GetExecutor();
		}
	} else if (CommandTemplate->LifecycleType == ECommandLifeTime::Game)
	{
		return CommandExecutor;
	} else
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot find command executor for the unknown command lifetime %s"),
			*UEnum::GetDisplayValueAsText(CommandTemplate->LifecycleType).ToString());
	}

	return nullptr;
}

void UGameConsole::ShowUI()
{
	if (Widget == nullptr)
	{
		if (WidgetClass.Get() != nullptr)
		{
			InitializeWidget();

			if (Widget == nullptr)
			{
				UE_LOG(LogTemp, Error, TEXT("Console widget initialization failed"));
				return;
			}
		} else
		{
			UE_LOG(LogTemp, Error, TEXT("Cannot construct console widget because widget class is not configured"));
			return;
		}
	}

	Widget->AddToViewport();
}

void UGameConsole::ProcessConsoleInput(const FString& Input)
{
	// TODO we probably should route commands through a PC component instead of finding the world in this fashion
	auto World = GetGameInstance()->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot process game console input because game world is missing"));
		return;
	}

	// TODO here we just blindly assume that the first local controller submitted the input
	const APlayerController* Requester = UGameplayStatics::GetPlayerController(World, 0);
		
	// is there a foreground command that waits for user input?
	if (ULatentConsoleCommand* ForegroundCommand = RunningInForeground.Get())
	{
		if (ForegroundCommand->IsWaitingForUserInput())
		{
			ForegroundCommand->PushUserInput(Input);
		} else
		{
			// simply swallow
			StdOut->Write(Input);
		}

		return;
	}

	// else - this must be a new command request
	ProcessConsoleCommand(Requester, Input);
}

void UGameConsole::AddCommandSet(const UConsoleCommandSet* CommandSet)
{
	Commands.Append(CommandSet->Commands);
}

bool UGameConsole::LookUpExecutionHistoryAtOffset(const int32 Offset, FString& HistoricalCommand) const
{
	if (Offset == 0)
	{
		return false;
	}

	const auto Offset_ = FMath::Abs(Offset);
	const int Idx = ExecutionHistory.Num() - Offset_;

	if (!ExecutionHistory.IsValidIndex(Idx))
	{
		return false;
	}

	HistoricalCommand = ExecutionHistory[Idx];
	return true;
}

void UGameConsole::Reset()
{
	CommandExecutor->Reset();
	ExecutionHistory.Empty();
	StdOut->Clear();
	StdErr->Clear();
}

void UGameConsole::GetKnownCommandNames(TArray<FString>& OutCommandNames, bool bContextRelevant) const
{
	for (const TSubclassOf<UConsoleCommandBase>& CommandCls : Commands)
	{
		if (!bContextRelevant || CommandCls.GetDefaultObject()->IsRelevantInCurrentContext())
		{
			OutCommandNames.Add(CommandCls.GetDefaultObject()->CommandName.ToString());
		}
	}
}

bool UGameConsole::ParseCommand(const FString& Command, UConsoleCommandBase*& CommandTemplate, FCommandArguments& OutArgs,
	FString& OutErrorMessage)
{
	if (!Tokenizer.Parse(Command))
	{
		OutErrorMessage = FString::Format(TEXT("Failed to tokenize the command with error: {0}"), {Tokenizer.GetError()->GetErrorMessage()});
		return false;
	}

	const auto ExistingCommand = FindCommandTemplate(FName(Tokenizer.GetTokens()[0]));
	if (!ExistingCommand)
	{
		OutErrorMessage = FString::Format(TEXT("Command with name {0} does not exist"), {Tokenizer.GetTokens()[0]});
		return false;
	}

	CommandTemplate = ExistingCommand;

	if (!Parser.ParseArguments(Tokenizer.GetTokens(), ExistingCommand, OutArgs, OutErrorMessage))
	{
		return false;
	}

	return true;
}

FAutocompletionRequest UGameConsole::ResolveAutocompletionRequest(const FString& AutocompletionString) const
{
	FAutocompletionTokenizer Tokenizer_;
	
	// try to parse the command to understand what exactly the user is trying to auto-complete
	if(!Tokenizer_.Parse(AutocompletionString))
	{
		UE_LOG(LogTemp, Error, TEXT("Tokenizer error while trying to get a list of auto-completions: %s"), *Tokenizer_.GetError()->GetErrorMessage());
		return FAutocompletionRequest::INVALID;
	}

	auto Tokens = MakeArrayView(Tokenizer_.GetTokens());
	
	// they are trying to auto-complete the command itself
	if (Tokens.Num() == 1)
	{
		return FAutocompletionRequest::CommandName(Tokens[0]);
	}

	// chop off the command name for simplicity
	const FName CommandName = FName(Tokens[0]);
	Tokens = Tokens.RightChop(1);

	// the user is trying to auto-complete parameter name or parameter value - in this case though we first need to know which command
	UConsoleCommandBase* Command = FindCommandTemplate(CommandName);

	if (!Command)
	{
		return FAutocompletionRequest::INVALID;
	}
	
	const FString TokenForAutocompletion = Tokens.Last();
	const int32 AutocompletionTokenIdx = Tokens.Num() - 1;

	if (Command->PositionalParameters.IsValidIndex(AutocompletionTokenIdx)) // trying to autocomplete a positional param
	{
		return FAutocompletionRequest::ParamValue(TokenForAutocompletion, Command, Command->PositionalParameters[AutocompletionTokenIdx]);
	}

	// consume positional parameters for convenience
	Tokens = Tokens.RightChop(Command->PositionalParameters.Num());
	
	// check first if we are trying to auto-complete a value of valid named parameter (preceding token is a valid parameter name)
	FCLIName ParamOrFlagName;
	if (Tokens.Num() > 1 && FCLIName::Parse(Tokens[Tokens.Num() - 2], ParamOrFlagName))
	{
		const UParameter* AutocompleteParameter;

		if (ParamOrFlagName.IsAlias())
		{
			AutocompleteParameter = Command->FindNamedParamByShortName(ParamOrFlagName.GetName());
		} else
		{
			AutocompleteParameter = Command->FindNamedParamByLongName(ParamOrFlagName.GetName());
		}

		if (AutocompleteParameter)
		{
			return FAutocompletionRequest::ParamValue(TokenForAutocompletion, Command, AutocompleteParameter);
		}
	}

	// check if the user is trying to auto-complete something that resembles a parameter or a flag name
	if (FCLIName::Parse(TokenForAutocompletion, ParamOrFlagName))
	{
		return FAutocompletionRequest::ParamName(ParamOrFlagName.GetName().ToString(), Command);
	}

	return FAutocompletionRequest::INVALID;
}

bool UGameConsole::RequestAutocompletions_Implementation(const FString& InString, TSet<FString>& AutoCompletions) const
{
	if (InString.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}
	
	// see if there's a foreground command waiting for user input - in this case we delegate auto-completion to the command
	if (const ULatentConsoleCommand* ForegroundCommand = RunningInForeground.Get())
	{
		if (ForegroundCommand->IsWaitingForUserInput() && ForegroundCommand->GetClass()->ImplementsInterface(UAutoCompletionProvider::StaticClass()))
		{
			return Execute_RequestAutocompletions(ForegroundCommand, InString, AutoCompletions);
		}

		return false;
	}
	
	FAutocompletionRequest AutocompletionRequest = ResolveAutocompletionRequest(InString);

	switch (AutocompletionRequest.GetType())
	{
	case Invalid:
		return false;
	case CommandName:
		{
			TArray<FString> CommandNames;
			GetKnownCommandNames(CommandNames);

			return UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(
				AutocompletionRequest.GetValue(),
				CommandNames, AutoCompletions);
		}
	case ParameterName:
		{
			TArray<FName> ParameterAndFlagNames;
			AutocompletionRequest.GetCommand()->GetParameterAndFlagNames(ParameterAndFlagNames);

			TArray<FString> ParameterAndFlagNamesStr;
			Algo::Transform(ParameterAndFlagNames, ParameterAndFlagNamesStr, [](const FName& Name) {return Name.ToString();});
		
			return UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(
				AutocompletionRequest.GetValue(),
				ParameterAndFlagNamesStr, AutoCompletions);
		}
	case ParameterValue:
		{
			const auto AutocompleteParam = AutocompletionRequest.GetParameter();

			if (AutocompleteParam->GetClass()->ImplementsInterface(UAutoCompletionProvider::StaticClass()))
			{
				return Execute_RequestAutocompletions(AutocompleteParam, InString, AutoCompletions);
			}
		}
	}
	
	return AutoCompletions.Num() > 0;
}

bool UGameConsole::SuggestAutoCompletions(const FString& InCmdText, const int32 CaretPosition,
	TSet<FString>& AutoCompletions) const
{
	if (InCmdText.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	if (CaretPosition < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Caret position cannot be negative"));
		return false;
	}

	// caret is at the start of the string - nothing to auto-complete
	if (CaretPosition == 0)
	{
		return false;
	}

	// no auto-completions when caret is preceded by whitespace
	if (InCmdText.IsValidIndex(CaretPosition - 1) && FChar::IsWhitespace(InCmdText[CaretPosition - 1]))
	{
		return false;
	}

	// there must be whitespace immediately after the caret for auto-completion to work (no auto-completion in the middle of words)
	if (CaretPosition < InCmdText.Len() && !FChar::IsWhitespace(InCmdText[CaretPosition]))
	{
		return false;
	}

	const auto SubstringForAutoCompletion = CaretPosition >= InCmdText.Len() ?
		InCmdText : InCmdText.Left(CaretPosition);
	return RequestAutocompletions_Implementation(SubstringForAutoCompletion, AutoCompletions);
}
