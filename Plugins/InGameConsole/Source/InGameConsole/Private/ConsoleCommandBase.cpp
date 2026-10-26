// Fill out your copyright notice in the Description page of Project Settings.


#include "ConsoleCommandBase.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"

void ULatentConsoleCommand::FinishExecutionAndCleanUp(EOnLatentCommandExecutionEndingReason Reason)
{
	bIsExecuting = false;
	bIsWaitingForUserInput = false;

	// let the blueprint also handle execution finish
	OnExecutionEnding(Reason);

	if (IsPersistent()) // non-persistent commands are simply destroyed by the executor, for persistent commands we need to cancel the latent actions
	{
		// a world is passed to each command's execution, and if it's null, the latent action manager had been already destroyed anyway
		if (CurrentExecutionContext.World.IsValid())
		{
			CurrentExecutionContext.World.Get()->GetLatentActionManager().RemoveActionsForObject(this);
		}

		UserInput->OnNewItem.Clear();
	}

	if (Reason == EOnLatentCommandExecutionEndingReason::ExecutionFinished)
	{
		OnExecutionFinished.Broadcast(); // TODO this could be one delegate taking the enum
	} else if (Reason == EOnLatentCommandExecutionEndingReason::ExecutionAborted)
	{
		OnCommandAborted.Broadcast();
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

const UParameter* UConsoleCommandBase::FindNamedParamByLongName(const FName& Name) const
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

const UParameter* UConsoleCommandBase::FindNamedParamByShortName(const FName& Name) const
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

const UParameter* UConsoleCommandBase::FindParameter(const FName& Name) const
{
	if (const auto Positional = PositionalParameters.FindByPredicate([Name](const UParameter* Parameter)
	{
		return Parameter->Name.IsEqual(Name);
	}))
	{
		return Positional->Get();
	}

	if (const auto Named = FindNamedParamByLongName(Name))
	{
		return Named;
	}

	return nullptr;
}

bool UConsoleCommandBase::ValidateArguments(const FCommandArguments& InArgs, FString& OutError) const
{
	// note: FCommandArguments must contain values for ALL argumnets, also optional ones; they must be populated by the parser

	// -- missing parameters
	auto PredicateMissing = [&InArgs](const UParameter* Parameter)
	{
		return Parameter->bRequired && !InArgs.HasArgumentWithName(Parameter->Name);
	};
	
	auto MissingParams = PositionalParameters.FilterByPredicate(PredicateMissing);
	MissingParams.Append(NamedParameters.FilterByPredicate(PredicateMissing));

	if (MissingParams.Num() > 0)
	{
		TArray<FString> MissingParamNames;
		Algo::Transform(MissingParams, MissingParamNames, [](const UParameter* Parameter)
		{
			return Parameter->Name.ToString();
		});
		
		OutError = FString::Format(TEXT("Missing arguments: {0}"), {FString::Join(MissingParamNames, TEXT(", "))});
		return false;
	}

	// -- unknown parameters or flags
	auto PredicateUnknown = [this](const FName& Name)
	{
		return FindNamedParamByLongName(Name) == nullptr &&
			!PositionalParameters.ContainsByPredicate([&Name](const UParameter* Parameter)
			{
				return Parameter->Name.IsEqual(Name);
			}) &&
			!Flags.Array().ContainsByPredicate([&Name](const FFlagDescription& Flag)
			{
				return Flag.Name.IsEqual(Name);
			});
	};

	TArray<FName> AllNames;
	InArgs.GetArgumentAndFlagNames(AllNames);
	
	TArray<FName> UnknownNames = AllNames.FilterByPredicate(PredicateUnknown);
	TArray<FString> UnknownNamesStr;
	Algo::Transform(UnknownNames, UnknownNamesStr, [](const FName& Name)
	{
		return Name.ToString();
	});

	if (UnknownNames.Num() > 0)
	{
		OutError = FString::Format(TEXT("Unknown argument names supplied: {0}"), {FString::Join(UnknownNamesStr, TEXT(", "))}); // TODO can we do this to an array of names directly?
		return false;
	}

	return true;
}

void UConsoleCommandBase::GetParameterAndFlagNames(TArray<FName>& Names) const
{
	for (const auto Param : NamedParameters)
	{
		Names.Add(Param->Name);
	}

	for (const auto Param : PositionalParameters)
	{
		Names.Add(Param->Name);
	}

	for (const auto Flag : Flags)
	{
		Names.Add(Flag.Name);
	}
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
