// Fill out your copyright notice in the Description page of Project Settings.


#include "ConsoleCommandBase.h"

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
				this->UserInput->OnNewItem.AddRaw(AwaitUserInputAction, &FLatentAwaitUserInputAction::AcceptUserInput);
			}

			// TODO
			/*// make the action listen to the abort events of this command
			if (this->OnCommandAborted.IsBound())
			{
				this->OnCommandAborted.Unbind();
				this->OnCommandAborted.BindRaw(AwaitUserInputAction, &FLatentAwaitUserInputAction::Abort);
			}*/
			
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
	
	bIsExecuting = false;
	this->ExitCode = InExitCode;
	OnExecutionFinished.ExecuteIfBound();
	// note: we don't kill the object, command lifetimes are managed by the command executor
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
	
	// TODO abortion logic?
	if (!IsPersistent())
	{
		// if command isn't persistent, it is sufficient to simply destroy the uobject (executor takes care) and unreal should take care of existing references
		OnCommandAborted.ExecuteIfBound();
	} else
	{
		// we should stop command execution but not destroy the command object
		
		// command could have latent actions registered with the world manager, we can try to kill them
		if (CurrentExecutionContext.Caller)
		{
			if (UWorld* World = CurrentExecutionContext.Caller->GetWorld())
			{
				World->GetLatentActionManager().RemoveActionsForObject(this);
			}
		}

		// the command could have subscribed to external actor/object events - for example, it can access a world actor and call 'Finish' on actor's take damage
		// TODO how do clear this type of references???

		// after all of the necessary cleanup - call command aborted
		OnCommandAborted.ExecuteIfBound();
	}
}

const UParameter* UConsoleCommandBase::FindNamedParamByLongName(const FName& Name)
{
	return NamedParameters.FindByPredicate([&Name](const UParameter* Parameter)
	{
		return Parameter->Name.IsEqual(Name);
	})->Get();
}

const UParameter* UConsoleCommandBase::FindNamedParamByShortName(const FName& Name)
{
	return NamedParameters.FindByPredicate([&Name](const UParameter* Parameter)
	{
		return Parameter->ShortName.IsEqual(Name);
	})->Get();
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
