// Fill out your copyright notice in the Description page of Project Settings.


#include "ConsoleCommandBase.h"

void ULatentConsoleCommand::FinishExecutionAndCleanUp(EOnLatentCommandExecutionEndingReason Reason)
{
	bIsExecuting = false;
	bIsWaitingForUserInput = false;

	// let the blueprint also handle execution finish
	OnExecutionEnding(Reason);

	if (IsPersistent()) // non-persistent commands are simply destroyed by the executor
	{
		if (CurrentExecutionContext.Caller) // TODO bad that this depends on the caller
		{
			CurrentExecutionContext.Caller->GetWorld()->GetLatentActionManager().RemoveActionsForObject(this);
		}

		UserInput->OnNewItem.Clear();
	}

	if (Reason == EOnLatentCommandExecutionEndingReason::ExecutionFinished)
	{
		OnExecutionFinished.ExecuteIfBound(); // TODO this could be one delegate taking the enum
	} else if (Reason == EOnLatentCommandExecutionEndingReason::ExecutionAborted)
	{
		OnCommandAborted.ExecuteIfBound();
	}
}

void ULatentConsoleCommand::ExecuteWithContext(const FCommandExecutionContext& Context)
{
	CurrentExecutionContext = Context;
	bIsExecuting = true;
	ExecuteImpl();
}

void ULatentConsoleCommand::RequireUserInput(const UObject* WorldContextObject,
                                             const FString& Prompt,
                                             FLatentActionInfo LatentInfo,
                                             FString& OutUserInput)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (World->GetLatentActionManager().FindExistingAction<FLatentAwaitUserInputAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			// push the prompt to stdout
			WriteMessageToStdOut(Prompt);
			
			bIsWaitingForUserInput = true; // TODO double-check the logic here
			FLatentAwaitUserInputAction* AwaitUserInputAction = new FLatentAwaitUserInputAction(OutUserInput, LatentInfo);

			// make the action listen to the input events of this command
			if (this->UserInput->OnNewItem.IsBound())
			{
				this->UserInput->OnNewItem.Clear();
			}
			this->UserInput->OnNewItem.AddRaw(AwaitUserInputAction, &FLatentAwaitUserInputAction::AcceptUserInput);
			
			World->GetLatentActionManager().AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, AwaitUserInputAction);
		}
	}
}

void ULatentConsoleCommand::Finish(const int32 InExitCode)
{
	if (!bIsExecuting)
	{
		UE_LOG(LogTemp, Error, TEXT("Called finished execution on the latent command but the command is not executing"));
		return;
	}
	
	this->ExitCode = InExitCode;
	FinishExecutionAndCleanUp(EOnLatentCommandExecutionEndingReason::ExecutionFinished);
}

bool ULatentConsoleCommand::PushUserInput(const FString& Input)
{
	if (bIsWaitingForUserInput)
	{
		bIsWaitingForUserInput = false;
		UserInput->Write(Input);
		return true;
	}

	return false;
}

void ULatentConsoleCommand::Abort()
{
	if (!bIsExecuting)
	{
		UE_LOG(LogTemp, Error, TEXT("Called 'Abort' on a latent command but the command is not executing"));
		return;
	}
	
	FinishExecutionAndCleanUp(EOnLatentCommandExecutionEndingReason::ExecutionAborted);
}

const UParameter* UConsoleCommandBase::FindNamedParamByLongName(const FName& Name)
{
	if (const TObjectPtr<UParameter>* Result = NamedParameters.FindByPredicate([&Name](const UParameter* Parameter)
	{
		return Parameter->Name.IsEqual(Name);
	}))
	{
		return Result->Get();
	}

	return nullptr;
}

const UParameter* UConsoleCommandBase::FindNamedParamByShortName(const FName& Name)
{
	if (const TObjectPtr<UParameter>* Result = NamedParameters.FindByPredicate([&Name](const UParameter* Parameter)
	{
		return Parameter->ShortName.IsEqual(Name);
	}))
	{
		return Result->Get();
	}

	return nullptr;
}

bool UConsoleCommandBase::CheckParamValueIsInAllowedSet(const FName& LongParamName, const FString& Value) const
{
	TSet<FString> AllowedValues;
	GetAllowedValues(LongParamName, AllowedValues);

	if (AllowedValues.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Empty set of allowed values returned for parameter with name '%s'"), *LongParamName.ToString());
		return false;
	}

	return AllowedValues.Contains(Value);
}
