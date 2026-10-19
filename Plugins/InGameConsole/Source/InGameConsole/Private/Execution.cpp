// Fill out your copyright notice in the Description page of Project Settings.


#include "Execution.h"
#include "ConsoleCommand.h"

void UCommandExecutor::HandleExecutionFinished(UConsoleCommand* Command, bool bForeground)
{
	if (bForeground && RunningInForeground == Command)
	{
		RunningInForeground = nullptr;
	}

	if (!bForeground && RunningInBackground.Contains(Command))
	{
		RunningInBackground.Remove(Command);
	}

	if (!Command->IsPersistent())
	{
		Command->MarkAsGarbage();
	}
}

bool UCommandExecutor::ExecuteInternal(const TSubclassOf<UConsoleCommand>& CommandClass, bool bForeground,
	const APlayerController* Requester, const FCommandArguments& Args)
{
	FCommandExecutionContext ExecutionContext;

	ExecutionContext.Caller = Requester;
	ExecutionContext.Arguments = Args;

	UConsoleCommand* CommandInstance;
		
	const auto CommandTemplate = CommandClass.GetDefaultObject();
	if (CommandTemplate->IsPersistent())
	{
		const auto Existing = PersistentCommands.FindByPredicate([&CommandClass](const UConsoleCommand* Command)
		{
			return Command->IsA(CommandClass.Get());
		});

		if (Existing != nullptr)
		{
			CommandInstance = *Existing;
		} else
		{
			CommandInstance = NewObject<UConsoleCommand>(this, CommandClass.Get());;
			PersistentCommands.Add(CommandInstance);
		}
	} else
	{
		CommandInstance = NewObject<UConsoleCommand>(this, CommandClass.Get());
	}

	if (!FindOutputChannelsForCommand(CommandInstance, bForeground, ExecutionContext.StdOut, ExecutionContext.StdErr))
	{
		CommandInstance->MarkAsGarbage();
		return false;
	}

	CommandInstance->ExecuteWithContext(ExecutionContext);

	// execution finished immediately
	if (CommandInstance->HasFinishedExecution())
	{
		if (!CommandInstance->IsPersistent())
		{
			CommandInstance->MarkAsGarbage();
		}
			
		return true;
	}

	// execution not finished in one frame
	if (bForeground)
	{
		RunningInForeground = CommandInstance;
	} else
	{
		RunningInBackground.Add(CommandInstance);
	}

	// listen to execution finish
	CommandInstance->OnExecutionFinished.BindUObject(
		this,
		&UCommandExecutor::HandleExecutionFinished,
		CommandInstance,
		bForeground);

	return true;
}

bool UCommandExecutor::ExecuteInBackground(const APlayerController* Requester,
	const TSubclassOf<UConsoleCommand>& CommandClass, const FCommandArguments& Args)
{
	if (GetRunningCommands().Num() == 0 || AllowsMultipleProcesses())
	{
		return ExecuteInternal(CommandClass, false, Requester, Args);
	}

	return false;
}

bool UCommandExecutor::ExecuteInForeground(const APlayerController* Requester,
	const TSubclassOf<UConsoleCommand>& CommandClass, const FCommandArguments& Args)
{
	if (GetCurrentForegroundCommand() != nullptr)
	{
		return false;
	}

	if (GetRunningCommands().Num() == 0 || AllowsMultipleProcesses())
	{
		return ExecuteInternal(CommandClass, true, Requester, Args);
	}

	return false;
}

TArray<UConsoleCommand*> UCommandExecutor::GetRunningCommands() const
{
	TArray<UConsoleCommand*> Running;
		
	Running.Append(RunningInBackground);

	if (RunningInForeground != nullptr)
	{
		Running.Add(RunningInForeground);
	}

	return Running;
}

void UCommandExecutor::Reset()
{
	for (UConsoleCommand* Persistent : PersistentCommands)
	{
		Persistent->MarkAsGarbage(); // TODO do we need special clean-ups or additional command-killing logic?
	}
	PersistentCommands.Empty();

	for (UConsoleCommand* BackgroundCommand : RunningInBackground)
	{
		BackgroundCommand->MarkAsGarbage();
	}
	RunningInBackground.Empty();

	if (RunningInForeground != nullptr)
	{
		RunningInForeground->MarkAsGarbage();
		RunningInForeground = nullptr;
	}
}
