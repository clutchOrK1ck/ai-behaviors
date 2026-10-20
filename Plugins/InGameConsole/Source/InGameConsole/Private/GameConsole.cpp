// Fill out your copyright notice in the Description page of Project Settings.

#include "GameConsole.h"
#include "ConsoleCommandBase.h"
#include "RemoteExecutionComponent.h"
#include "Kismet/GameplayStatics.h"

bool UGameConsoleCommandExecutor::FindOutputChannelsForCommand(const UConsoleCommandBase* ConsoleCommand,
                                                                         bool bIsExecutingInForeground, ITextPipe*& OutStdOut, ITextPipe*& OutStdErr)
{
	if (!bIsExecutingInForeground)
	{
		return false;
	}

	OutStdOut = Console->StdOut;
	OutStdErr = Console->StdErr;

	return true;
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
	Widget = CreateWidget<UConsoleWidget>(this->GetGameInstance(), WidgetClass.Get());

	// make the widget listen to stdout and stderr of the game console
	this->StdOut->OnNewLine.AddLambda([this](const FString& Line)
	{
		Widget->OnNewLine(Line, EConsoleLineType::Display);
	});

	this->StdErr->OnNewLine.AddLambda([this](const FString& Line)
	{
		Widget->OnNewLine(Line, EConsoleLineType::Error);
	});
}

void UGameConsole::ProcessConsoleCommand(const APlayerController* Requester, const FString& Command)
{
	FString ErrorMsg;
		
	if (!Tokenizer.Parse(Command))
	{
		StdErr->Write(FString::Format(TEXT("Failed to tokenize the command with error: {0}"), {Tokenizer.GetError()->GetErrorMessage()}));
		return;
	}

	const auto CommandTemplate = FindCommandTemplate(FName(Tokenizer.GetTokens()[0]));
	if (!CommandTemplate)
	{
		StdErr->Write("Command bla bla does not exist"); // TODO
		return;
	}

	FCommandArguments Args;

	if (!Parser.ParseArguments(Tokenizer.GetTokens(), CommandTemplate, Args, ErrorMsg))
	{
		StdErr->Write("Bla bla syntax error"); // TODO
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
		
		RemoteExecutionComponent->ExecuteCommandOnServer(CommandTemplate->StaticClass(), Args);
	} else
	{
		CommandExecutor->ExecuteInForeground(Requester, CommandTemplate->StaticClass(), Args);
	}

	ExecutionHistory.Add(Command);
}

void UGameConsole::ShowUI()
{
	if (Widget == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Console widget is null when 'ShowUI' is called"));
		InitializeWidget();
	}

	Widget->AddToViewport();
}

void UGameConsole::ProcessConsoleInput(const FString& Input)
{
	// TODO double-check this logic
	auto World = GetGameInstance()->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot process game console input because game world is missing"));
		return;
	}
		
	const APlayerController* Requester = UGameplayStatics::GetPlayerController(World, 0);
		
	// is there a foreground command that waits for user input?
	if (ULatentConsoleCommand* ForegroundCommand = CommandExecutor->GetCurrentForegroundCommand())
	{
		if (ForegroundCommand->IsWaitingForUserInput())
		{
			ForegroundCommand->PushUserInput(Input);
		} else
		{
			// simply swallow
			StdOut->Write(Input);
		}
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

bool UGameConsole::RequestAutocompletions_Implementation(const FString& InString, TSet<FString>& AutoCompletions) const
{
	if (InString.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}
	
	// see if there's a foreground command waiting for user input - in this case we delegate auto-completion to the command
	if (const ULatentConsoleCommand* ForegroundCommand = CommandExecutor->GetCurrentForegroundCommand())
	{
		if (ForegroundCommand->IsWaitingForUserInput() && ForegroundCommand->StaticClass()->ImplementsInterface(UAutoCompletionProvider::StaticClass()))
		{
			return Execute_RequestAutocompletions(ForegroundCommand, InString, AutoCompletions);
		}

		return false;
	}

	FTokenizer Tokenizer_;
	
	// try to parse the command to understand what exactly the user is trying to auto-complete
	if(!Tokenizer_.Parse(InString))
	{
		UE_LOG(LogTemp, Error, TEXT("Tokenizer error while trying to get a list of auto-completions: %s"), *Tokenizer.GetError()->GetErrorMessage());
		return false;
	}

	const TArray<FString>& Tokens = Tokenizer.GetTokens();
	
	// they are trying to auto-complete the command itself
	if (Tokens.Num() == 1)
	{
		TArray<FString> CommandNames;
		GetKnownCommandNames(CommandNames);

		return UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(InString, CommandNames, AutoCompletions);
	}

	// the user is trying to auto-complete parameter name or parameter value - in this case though we first need to know which command
	UConsoleCommandBase* Command = FindCommandTemplate(FName(Tokenizer.GetTokens()[0]));

	if (!Command)
	{
		return false;
	}

	// parameter to auto-complete value of, if user is trying to auto-complete a parameter value
	const UParameter* AutoCompleteParam = nullptr;

	if (const bool bIsTryingToAutocompletePositionalParam = Tokenizer.GetTokens().Num() <= Command->PositionalParameters.Num() + 1) // positional param auto-completion?
	{
		AutoCompleteParam = Command->PositionalParameters[Tokenizer.GetTokens().Num() - 2];
	} else
	{
		// we are trying to auto-complete a named param if the preceding token is the long or the short name of an existing parameter
		FString PrecedingToken = Tokenizer.GetTokens()[Tokenizer.GetTokens().Num() - 2];

		if (Tokenizer.GetTokens().Num() > Command->PositionalParameters.Num() + 1)
		{
			if (PrecedingToken.StartsWith("--")
			&& Command->FindNamedParamByLongName(FName(PrecedingToken.TrimChar('-').TrimChar('-'))))
			{
				AutoCompleteParam = Command->FindNamedParamByLongName(FName(PrecedingToken.TrimChar('-').TrimChar('-')));
			} else if (PrecedingToken.StartsWith("-")
				&& Command->FindNamedParamByShortName(FName(PrecedingToken.TrimChar('-'))))
			{
				AutoCompleteParam = Command->FindNamedParamByShortName(FName(PrecedingToken.TrimChar('-')));
			}
		}
	}

	if (AutoCompleteParam && AutoCompleteParam->StaticClass()->ImplementsInterface(UAutoCompletionProvider::StaticClass()))
	{
		return Execute_RequestAutocompletions(AutoCompleteParam, InString, AutoCompletions);
	}

	// finally, maybe the user is trying to auto-complete a named parameter or a flag name
	// TODO

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
	if (FChar::IsWhitespace(InCmdText[CaretPosition - 1]))
	{
		return false;
	}

	// there must be whitespace immediately after the caret for auto-completion to work (no auto-completion in the middle of words)
	if (CaretPosition < InCmdText.Len() && !FChar::IsWhitespace(InCmdText[CaretPosition]))
	{
		return false;
	}

	const auto SubstringForAutoCompletion = CaretPosition >= InCmdText.Len() ?
		InCmdText : InCmdText.Mid(0, CaretPosition + 1);
	return RequestAutocompletions_Implementation(SubstringForAutoCompletion, AutoCompletions);
}
