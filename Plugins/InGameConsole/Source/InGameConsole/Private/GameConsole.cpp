// Fill out your copyright notice in the Description page of Project Settings.

#include "GameConsole.h"
#include "ConsoleCommandBase.h"
#include "LocalizationDescriptor.h"
#include "RemoteExecutionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Widgets/SViewport.h"
#include "Widgets/SGameConsole.h"

/** Handle the configured key before UConsole consumes Tilde. Scoped to this game's focused viewport. */
class FGameConsoleInputProcessor : public IInputProcessor
{
public:

	explicit FGameConsoleInputProcessor(UGameConsole* InConsole) : Console(InConsole)
	{
	}

	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		UGameConsole* Owner = Console.Get();
		if (!Owner || Event.GetKey() != GetDefault<UGameConsoleSettings>()->ToggleKey
			|| Event.IsControlDown() || Event.IsAltDown() || Event.IsCommandDown())
		{
			return false;
		}

		UGameViewportClient* Viewport = Owner->GetGameInstance()->GetGameViewportClient();
		TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
		const bool bViewportFocused = ViewportWidget.IsValid()
			&& (ViewportWidget->HasKeyboardFocus() || ViewportWidget->HasFocusedDescendants());
		const bool bConsoleFocused = Owner->Widget.IsValid() && Owner->Widget->HasFocusedDescendants();
		if (!bViewportFocused && !bConsoleFocused)
		{
			return false;
		}

		if (!Event.IsRepeat())
		{
			Owner->bToggleKeyPressed = true;
			Owner->ToggleUI();
		}

		return true;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		if (UGameConsole* Owner = Console.Get())
		{
			if (Event.GetKey() == GetDefault<UGameConsoleSettings>()->ToggleKey)
			{
				const bool bHandled = Owner->bToggleKeyPressed;
				Owner->bToggleKeyPressed = false;
				return bHandled;
			}
		}

		return false;
	}

private:

	TWeakObjectPtr<UGameConsole> Console;
};

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

	if (!NewPlayerController || !GetWorld() || !GetWorld()->GetGameInstance())
	{
		return;
	}

	if (UGameConsole* Console = GetWorld()->GetGameInstance()->GetSubsystem<UGameConsole>())
	{
		Console->HideUI();
		NewPlayerController->PushInputComponent(Console->GetInputComponent());
	} else
	{
		UE_LOG(LogTemp, Error, TEXT("Error while trying to push console input component: game console subsystem not found"));
	}
}

UConsoleCommandBase* UGameConsole::GetCommandTemplate(const FName& CommandName, bool ContextRelevant) const
{
	for (auto ConsoleCommand : CommandTemplates)
	{
		if (ConsoleCommand->CommandName.IsEqual(CommandName) && ConsoleCommand->IsRelevantInCurrentContext())
		{
			return ConsoleCommand;
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

	StdOut->OnNewItem.AddUObject(this, &UGameConsole::AppendOutput, EConsoleLineType::Display);
	StdErr->OnNewItem.AddUObject(this, &UGameConsole::AppendOutput, EConsoleLineType::Error);

	if (FSlateApplication::IsInitialized() && !IsRunningDedicatedServer())
	{
		SlateInputProcessor = MakeShared<FGameConsoleInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(SlateInputProcessor, 0);
	}
}

void UGameConsole::Deinitialize()
{
	HideUI();
	if (SlateInputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(SlateInputProcessor);
	}

	SlateInputProcessor.Reset();
	Widget.Reset();
	StdOut->OnNewItem.RemoveAll(this);
	StdErr->OnNewItem.RemoveAll(this);
	Super::Deinitialize();
}

void UGameConsole::AppendOutput(const FString& Text, EConsoleLineType Type)
{
	const FConsoleOutputLine Line{Text, Type};
	OutputLines.Add(Line);
	OnOutput.Broadcast(Line);
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
			&UGameConsole::ToggleUI);
	}
}

void UGameConsole::OnForegroundCommandFinished()
{
	RunningInForeground.Reset();
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

				// non-persistent commands get destroyed after execution finishes, so the foreground pointer becomes invalid
				// but for persistent commands (level and world commands) we need to clear it
				if (CommandHandle->IsPersistent() && !CommandHandle->OnExecutionFinished.IsBoundToObject(this))
				{
					RunningInForeground->OnExecutionFinished.AddUObject(
						this,
						&UGameConsole::OnForegroundCommandFinished);

					RunningInForeground->OnCommandAborted.AddUObject(
						this,
						&UGameConsole::OnForegroundCommandFinished);
				}
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
	if (bUIVisible || !FSlateApplication::IsInitialized())
	{
		return;
	}

	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!Viewport)
	{
		return;
	}

	if (!Widget.IsValid())
	{
		Widget = SNew(SGameConsole).Console(this);
	}

	PreviousKeyboardFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	WidgetViewport = Viewport;
	bUIVisible = true;
	if (APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController())
	{
		UIController = Controller;
		bPreviousMouseCursor = Controller->bShowMouseCursor;
		Controller->bShowMouseCursor = true;
		Controller->SetIgnoreLookInput(true);
		Controller->SetIgnoreMoveInput(true);
	}

	Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 10000);
	FSlateApplication::Get().ReleaseAllPointerCapture();
	Widget->FocusInput();
}

void UGameConsole::HideUI()
{
	if (!bUIVisible)
	{
		return;
	}

	bUIVisible = false;
	if (Widget.IsValid())
	{
		Widget->DismissCompletions();
		if (UGameViewportClient* Viewport = WidgetViewport.Get())
		{
			Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}

	if (APlayerController* Controller = UIController.Get())
	{
		Controller->bShowMouseCursor = bPreviousMouseCursor;
		Controller->SetIgnoreLookInput(false);
		Controller->SetIgnoreMoveInput(false);
	}

	UIController.Reset();
	WidgetViewport.Reset();
	if (FSlateApplication::IsInitialized())
	{
		if (TSharedPtr<SWidget> Previous = PreviousKeyboardFocus.Pin())
		{
			FSlateApplication::Get().SetKeyboardFocus(Previous);
		} else
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}

	PreviousKeyboardFocus.Reset();
}

void UGameConsole::ToggleUI()
{
	if (bUIVisible)
	{
		HideUI();
	} else
	{
		ShowUI();
	}
}

void UGameConsole::ProcessConsoleInput(const FString& Input)
{
	if (Input.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	AppendOutput(TEXT("> ") + Input, EConsoleLineType::Input);

	// TODO we probably should route commands through a PC component instead of finding the world in this fashion
	auto World = GetGameInstance()->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot process game console input because game world is missing"));
		return;
	}

	// TODO here we just blindly assume that the first local controller submitted the input
	const APlayerController* Requester = UGameplayStatics::GetPlayerController(World, 0);
	if (!Requester)
	{
		StdErr->Write(TEXT("Cannot execute console input without a player controller."));
		return;
	}

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
	for (const auto& CommandClass : CommandSet->Commands)
	{
		if (!CommandTemplates.ContainsByPredicate([&CommandClass](const UConsoleCommandBase* Command)
		{
			return Command->GetClass() == CommandClass.Get();
		}))
		{
			const auto Template = NewObject<UConsoleCommandBase>(this, CommandClass.Get());
			CommandTemplates.Add(Template);
		}
	}
}

TArray<FCommandDefinition> UGameConsole::GetCommandDefinitions() const
{
	TArray<FCommandDefinition> Definitions;

	for (const auto Command : CommandTemplates)
	{
		if (Command->IsRelevantInCurrentContext())
		{
			Definitions.Add(Command->GetCommandDefinition());
		}
	}

	return Definitions;
}

void UGameConsole::FindCommandDefinition(FName CommandName, FCommandDefinition& Definition, bool& Found) const
{
	Found = false;
	
	for (const auto& Definition_ : GetCommandDefinitions())
	{
		if (Definition_.Name.IsEqual(CommandName))
		{
			Found = true;
			Definition = Definition_;
			return;
		}
	}
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

	if (UWorld* World = GetWorld())
	{
		if (const auto CommandExecutionWorldSubsystem = World->GetSubsystem<UCommandExecutionWorldSubsystem>())
		{
			CommandExecutionWorldSubsystem->GetExecutor()->Reset();
		}
	}

	RunningInForeground.Reset();
	OutputLines.Empty();
	OnOutputCleared.Broadcast();
}

void UGameConsole::GetKnownCommandNames(TArray<FString>& OutCommandNames, bool bContextRelevant) const
{
	for (const auto Command : CommandTemplates)
	{
		if (!bContextRelevant || Command->IsRelevantInCurrentContext())
		{
			OutCommandNames.Add(Command->CommandName.ToString());
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

	const auto ExistingCommand = GetCommandTemplate(FName(Tokenizer.GetTokens()[0]));
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
	UConsoleCommandBase* Command = GetCommandTemplate(CommandName);

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
		// refuse to auto-complete short names because those are usually one char anyway
		if (ParamOrFlagName.IsAlias())
		{
			return FAutocompletionRequest::INVALID;
		}

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
			// suggest autocompletions for *named* parameters and flags
			TArray<FString> ParameterAndFlagNames;

			for (const auto NamedParam : AutocompletionRequest.GetCommand()->NamedParameters)
			{
				ParameterAndFlagNames.Add(NamedParam->Name.ToString());
			}

			for (const auto Flag : AutocompletionRequest.GetCommand()->Flags)
			{
				ParameterAndFlagNames.Add(Flag.Name.ToString());
			}

			return UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(
				AutocompletionRequest.GetValue(),
				ParameterAndFlagNames, AutoCompletions);
		}

	case ParameterValue:
		{
			const auto AutocompleteParam = AutocompletionRequest.GetParameter();

			// dynamically provided string values
			if (AutocompleteParam->IsA<UStringParameter>() || AutocompleteParam->IsA<UListParameter>())
			{
				FStringValidationSettings Validation = AutocompleteParam->IsA<UStringParameter>()
					                                        ? Cast<UStringParameter>(AutocompleteParam)->Validation
					                                        : Cast<UListParameter>(AutocompleteParam)->Validation;

				if (Validation.ValidationType == AllowedValueSet && Validation.bProvideAllowedValuesDynamically)
				{
					TSet<FString> AllowedValues;

					AutocompletionRequest.GetCommand()->GetAllowedValues(AutocompleteParam->Name, AllowedValues);

					return UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(
						AutocompletionRequest.GetValue(),
						AllowedValues.Array(), AutoCompletions);
				}
			}
			
			if (AutocompleteParam->GetClass()->ImplementsInterface(UAutoCompletionProvider::StaticClass()))
			{
				return Execute_RequestAutocompletions(AutocompleteParam, AutocompletionRequest.GetValue(), AutoCompletions);
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
