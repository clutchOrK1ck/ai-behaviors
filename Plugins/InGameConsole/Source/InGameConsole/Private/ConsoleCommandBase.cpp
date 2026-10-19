// Fill out your copyright notice in the Description page of Project Settings.


#include "ConsoleCommandBase.h"

void ULatentConsoleCommand::RequireUserInput(const UObject* WorldContextObject,
                                           const FString& Prompt,
                                           FLatentActionInfo LatentInfo,
                                           FString& OutUserInput,
                                           EReadUserInputResult& Result)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (World->GetLatentActionManager().FindExistingAction<FLatentAwaitUserInputAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == NULL)
		{
			// push the prompt to stdout
			WriteMessageToStdOut(Prompt);
			
			bIsWaitingForUserInput = true; // TODO double-check the logic here
			FLatentAwaitUserInputAction* AwaitUserInputAction = new FLatentAwaitUserInputAction(Result, LatentInfo);

			// make the action listen to the input events of this command
			if (this->UserInput->OnNewInput.IsBound())
			{
				this->UserInput->OnNewInput.Clear();
				this->UserInput->OnNewInput.AddRaw(AwaitUserInputAction, &FLatentAwaitUserInputAction::AcceptUserInput);
			}

			// make the action listen to the abort events of this command
			if (this->OnCommandAborted.IsBound())
			{
				this->OnCommandAborted.Unbind();
				this->OnCommandAborted.BindRaw(AwaitUserInputAction, &FLatentAwaitUserInputAction::Abort);
			}
			
			World->GetLatentActionManager().AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, AwaitUserInputAction);
		}
	}
}

void ULatentConsoleCommand::Finish(const int32 InExitCode)
{
	bHasFinished = true;
	this->ExitCode = InExitCode;
	OnExecutionFinished.ExecuteIfBound();

	// note: we don't kill the object, command lifetimes are managed by the command executor
}

bool ULatentConsoleCommand::IsWaitingForUserInput() const
{
	return bIsWaitingForUserInput;
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
