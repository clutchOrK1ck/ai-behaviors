// Fill out your copyright notice in the Description page of Project Settings.


#include "Execution.h"
#include "ConsoleCommandBase.h"

void UCommandExecutor::HandleExecutionFinished(ULatentConsoleCommand* Command, bool bForeground)
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

bool UCommandExecutor::ExecuteInternal(const TSubclassOf<UConsoleCommandBase>& CommandClass, bool bForeground,
                                       const APlayerController* Requester, const FCommandArguments& Args)
{
	if (CommandClass->IsChildOf(ULatentConsoleCommand::StaticClass()) && !CanExecuteLatentCommands())
	{
		return false;
	}
	
	FCommandExecutionContext ExecutionContext;

	ExecutionContext.Caller = Requester;
	ExecutionContext.Arguments = Args;

	UConsoleCommandBase* CommandInstance;
		
	const auto CommandTemplate = CommandClass.GetDefaultObject();
	if (CommandTemplate->IsPersistent())
	{
		const auto Existing = PersistentCommands.FindByPredicate([&CommandClass](const UConsoleCommandBase* Command)
		{
			return Command->IsA(CommandClass.Get());
		});

		if (Existing != nullptr)
		{
			CommandInstance = *Existing;
		} else
		{
			CommandInstance = NewObject<UConsoleCommandBase>(this, CommandClass.Get());;
			PersistentCommands.Add(CommandInstance);
		}
	} else
	{
		CommandInstance = NewObject<UConsoleCommandBase>(this, CommandClass.Get());
	}

	if (!FindOutputChannelsForCommand(CommandInstance, bForeground, ExecutionContext.StdOut, ExecutionContext.StdErr))
	{
		CommandInstance->MarkAsGarbage();
		return false;
	}

	CommandInstance->ExecuteWithContext(ExecutionContext);

	if (const auto Latent = Cast<ULatentConsoleCommand>(CommandInstance))
	{
		// execution finished immediately
		if (Latent->HasFinishedExecution())
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
			RunningInForeground = Latent;
		} else
		{
			RunningInBackground.Add(Latent);
		}

		// listen to execution finish
		Latent->OnExecutionFinished.BindUObject(
			this,
			&UCommandExecutor::HandleExecutionFinished,
			Latent,
			bForeground);

		// listen to abortion
		Latent->OnCommandAborted.BindUObject(
			this,
			&UCommandExecutor::HandleExecutionFinished,
			Latent,
			bForeground);
	}

	return true;
}

bool UCommandExecutor::ExecuteInBackground(const APlayerController* Requester,
	const TSubclassOf<UConsoleCommandBase>& CommandClass, const FCommandArguments& Args)
{
	if (GetRunningCommands().Num() == 0 || AllowsMultipleProcesses())
	{
		return ExecuteInternal(CommandClass, false, Requester, Args);
	}

	return false;
}

bool UCommandExecutor::ExecuteInForeground(const APlayerController* Requester,
	const TSubclassOf<UConsoleCommandBase>& CommandClass, const FCommandArguments& Args)
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

TArray<ULatentConsoleCommand*> UCommandExecutor::GetRunningCommands() const
{
	TArray<ULatentConsoleCommand*> Running;
		
	Running.Append(RunningInBackground);

	if (RunningInForeground != nullptr)
	{
		Running.Add(RunningInForeground);
	}

	return Running;
}

void UCommandExecutor::Reset()
{
	for (UConsoleCommandBase* Persistent : PersistentCommands)
	{
		Persistent->MarkAsGarbage(); // TODO do we need special clean-ups or additional command-killing logic?
	}
	PersistentCommands.Empty();

	for (UConsoleCommandBase* BackgroundCommand : RunningInBackground)
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
