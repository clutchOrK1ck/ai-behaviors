// Fill out your copyright notice in the Description page of Project Settings.


#include "Execution.h"
#include "ConsoleCommandBase.h"
#include "Async/IAsyncTask.h"

UConsoleCommandBase* UCommandExecutor::GetOrCreateCommandInstance(TSubclassOf<UConsoleCommandBase> CommandClass)
{
	const auto CommandTemplate = CommandClass.GetDefaultObject();
	if (CommandTemplate->IsPersistent())
	{
		const auto Existing = PersistentInstances.FindByPredicate([&CommandClass](const UConsoleCommandBase* Command)
		{
			return Command->GetClass() == CommandClass.Get();
		});

		if (Existing != nullptr)
		{
			return *Existing;
		}
		
		auto NewInstance = NewObject<UConsoleCommandBase>(this, CommandClass.Get());;
		PersistentInstances.Add(NewInstance);
		return NewInstance;
	}
	
	return NewObject<UConsoleCommandBase>(this, CommandClass.Get());
}

void UCommandExecutor::HandleExecutionFinished(ULatentConsoleCommand* Command)
{	
	if (RunningLatentCommands.Contains(Command))
	{
		RunningLatentCommands.Remove(Command);
	} else
	{
		UE_LOG(LogTemp, Error, TEXT("Latent command that finished execution not in the list of running commands"));
	}
	
	if (!Command->IsPersistent())
	{
		Command->MarkAsGarbage();
	}
}

bool UCommandExecutor::Execute(const APlayerController* Requester, const TSubclassOf<UConsoleCommandBase>& CommandClass,
	const FCommandArguments& Args, ITextPipe* StdOut, ITextPipe* StdErr)
{
	ULatentConsoleCommand* SwallowPtr;
	return Execute(Requester, CommandClass, Args, StdOut, StdErr, SwallowPtr);
}

bool UCommandExecutor::Execute(const APlayerController* Requester,
                               const TSubclassOf<UConsoleCommandBase>& CommandClass,
                               const FCommandArguments& Args,
                               ITextPipe* StdOut,
                               ITextPipe* StdErr,
                               ULatentConsoleCommand*& OutCommandHandle)
{
	OutCommandHandle = nullptr;
	
	if (GetRunningCommands().Num() > 0 && !bAllowsParallelExecution)
	{
		return false;
	}

	if (CommandClass->IsChildOf(ULatentConsoleCommand::StaticClass()) && !bCanExecuteLatentCommands)
	{
		return false;
	}
	
	FCommandExecutionContext ExecutionContext;

	ExecutionContext.Caller = Requester;
	ExecutionContext.Arguments = Args;
	ExecutionContext.StdOut = StdOut;
	ExecutionContext.StdErr = StdErr;

	UConsoleCommandBase* CommandInstance = GetOrCreateCommandInstance(CommandClass);
	CommandInstance->ExecuteWithContext(ExecutionContext);

	if (const auto Latent = Cast<ULatentConsoleCommand>(CommandInstance))
	{
		// execution finished immediately
		if (Latent->HasFinishedExecution())
		{
			if (!CommandInstance->IsPersistent())
			{
				CommandInstance->MarkAsGarbage(); // destroy after execution
			}
			
			return true;
		}
		
		RunningLatentCommands.Add(Latent);
		
		// listen to execution finish
		Latent->OnExecutionFinished.BindUObject(
			this,
			&UCommandExecutor::HandleExecutionFinished,
			Latent);

		// listen to abortion
		Latent->OnCommandAborted.BindUObject(
			this,
			&UCommandExecutor::HandleExecutionFinished,
			Latent);

		OutCommandHandle = Latent;
	}

	return true;
}

void UCommandExecutor::Reset()
{
	for (auto RunningCommand : GetRunningCommands())
	{
		RunningCommand->Abort();
	}

	for (auto Persistent : PersistentInstances)
	{
		Persistent->MarkAsGarbage();
	}
	PersistentInstances.Empty();
}
