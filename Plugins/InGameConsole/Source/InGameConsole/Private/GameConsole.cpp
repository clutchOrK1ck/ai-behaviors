// Fill out your copyright notice in the Description page of Project Settings.

#include "GameConsole.h"
#include "ConsoleCommand.h"
#include "Kismet/GameplayStatics.h"

bool UGameConsoleCommandExecutor::FindOutputChannelsForBackgroundCommand(const UConsoleCommand* ConsoleCommand,
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
	if (UConsoleCommand* ForegroundCommand = CommandExecutor->GetCurrentForegroundCommand())
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
		
	FString ErrorMsg;
		
	if (!Tokenizer.Parse(Input))
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
	}

	CommandExecutor->ExecuteInForeground(Requester, CommandTemplate->StaticClass(), Args);
	ExecutionHistory.Add(Input);
}

void UGameConsole::AddCommandSet(const UConsoleCommandSet* CommandSet)
{
	Commands.Append(CommandSet->Commands);
}

bool UGameConsole::LookUpExecutionHistoryAtOffset(const uint32 Offset, FString& HistoricalCommand) const
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

void UGameConsole::GetKnownCommandNames(TArray<FString>& OutCommandNames) const
{
	// TODO
}

bool UGameConsole::RequestAutocompletions_Implementation(const FString& InString, TSet<FString>& AutoCompletions) const
{
	if (InString.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}
	
	// see if there's a foreground command waiting for user input - in this case we delegate auto-completion to the command
	if (const UConsoleCommand* ForegroundCommand = CommandExecutor->GetCurrentForegroundCommand())
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
	UConsoleCommand* Command = FindCommandTemplate(FName(Tokenizer.GetTokens()[0]));

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
