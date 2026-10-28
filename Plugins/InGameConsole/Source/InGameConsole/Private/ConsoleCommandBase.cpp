// Fill out your copyright notice in the Description page of Project Settings.


#include "ConsoleCommandBase.h"

#include "Execution.h"
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

void ULatentConsoleCommand::GenerateCommandDefinition(FCommandDefinition& OutCommandDefinition) const
{
	Super::GenerateCommandDefinition(OutCommandDefinition);
	OutCommandDefinition.bLatent = true;
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

FString ToString(const FIntegerValidationSettings& ValidationSettings)
{
	if (ValidationSettings.MinValue == 0 && ValidationSettings.MaxValue == MAX_int32)
	{
		return "Restricted to positive integer values";
	}

	if (ValidationSettings.MaxValue == 0 && ValidationSettings.MinValue == MIN_int32)
	{
		return "Restricted to negative integer values";
	}
	
	if (ValidationSettings.bRestrictRange)
	{
		return FString::Format(TEXT("Restricted to integer values in range: [{0}, {1}]"),
			{LexToString(ValidationSettings.MinValue), LexToString(ValidationSettings.MaxValue)});
	}

	return "";
}

FString ToString(const FStringValidationSettings& ValidationSettings)
{
	if (ValidationSettings.ValidationType == AllowedValueSet && ValidationSettings.bProvideAllowedValuesDynamically)
	{
		return "This argument is restricted to a set of allowed values. Use auto-completion to check"; // TODO at this point we have no reference to the command
	}
	
	switch(ValidationSettings.ValidationType)
	{
	case Regex:
		return FString::Format(TEXT("Supplied values must match the regex expression: {0}"), {ValidationSettings.Regex});
	case AllowedValueSet:
		{
			auto AllowedValues = MakeArrayView(ValidationSettings.AllowedValues.Array());

			AllowedValues = AllowedValues.LeftChop(10); // TODO this probably must be configurable
		
			return FString::Format(TEXT("Supplied values must be in the set: \\{{0}\\}"),
				{FString::Join(AllowedValues, TEXT(" "))});
		}
	case None:
	default:
		return "";
	}
}

FString ToString(const FRealValidationSettings& ValidationSettings)
{
	if (ValidationSettings.bRestrictRange)
	{
		// only positive values
		if (FMath::IsNearlyZero(ValidationSettings.MinValue) && FMath::IsNearlyEqual(ValidationSettings.MaxValue, MAX_dbl, 1.))
		{
			return "Restricted to positive real values";
		}

		// only negative values
		if (FMath::IsNearlyZero(ValidationSettings.MaxValue) && FMath::IsNearlyEqual(ValidationSettings.MinValue, MIN_dbl, 1.))
		{
			return "Restricted to negative real values";
		}
		
		return FString::Format(TEXT("Restricted to real values in range: [{0}, {1}]"), {
			                       LexToString(ValidationSettings.MinValue),
			                       LexToString(ValidationSettings.MaxValue)
		                       });
	}

	return "";
}

FCommandParameterDefinition UConsoleCommandBase::MakeParameterDefinition(const UParameter* Parameter) const
{
	FCommandParameterDefinition Result;

	Result.Name = Parameter->Name;
	Result.Alias = Parameter->ShortName;
	Result.Description = Parameter->Description;
	Result.bRequired = Parameter->bRequired;

	if (const auto BoolParam = Cast<UBoolParameter>(Parameter))
	{
		Result.ValueType = EParameterValueType::Boolean;
		Result.DefaultValue = LexToString(BoolParam->GetDefaultValue());
	} else if (const auto IntParam = Cast<UIntegerParameter>(Parameter))
	{
		Result.ValueType = EParameterValueType::Integer;
		Result.DefaultValue = LexToString(IntParam->GetDefaultValue());
		Result.ValidationInfo = ToString(IntParam->Validation);
	} else if (const auto StringParam = Cast<UStringParameter>(Parameter))
	{
		Result.ValueType = EParameterValueType::String;
		Result.DefaultValue = LexToString(StringParam->DefaultValue);
		Result.ValidationInfo = ToString(StringParam->Validation);
	} else if (const auto RealParam = Cast<URealParameter>(Parameter))
	{
		Result.ValueType = EParameterValueType::Real;
		Result.DefaultValue = LexToString(RealParam->GetDefaultValue());
		Result.ValidationInfo = ToString(RealParam->Validation);
	} else if (const auto VectorParam = Cast<UVectorParameter>(Parameter))
	{
		Result.ValueType = EParameterValueType::Vector;
		Result.DefaultValue = VectorParam->GetDefaultValue().ToString();
	} else if (const auto ListParam = Cast<UListParameter>(Parameter))
	{
		Result.ValueType = EParameterValueType::List;
		Result.DefaultValue = FString("[") + FString::Join(
			ListParam->GetDefaultValue(), TEXT(" ")) + "]";
		Result.ValidationInfo = ToString(ListParam->Validation);
	}

	return Result;
}

void UConsoleCommandBase::FillParameterAndFlagDefinitions(FCommandDefinition& OutCommandDefinition) const
{
	for (const UParameter* Parameter : PositionalParameters)
	{
		OutCommandDefinition.PositionalParameters.Add(MakeParameterDefinition(Parameter));
	}

	for (const UParameter* Parameter : NamedParameters)
	{
		OutCommandDefinition.NamedParameters.Add(MakeParameterDefinition(Parameter));
	}

	for (const auto& Flag : Flags)
	{
		OutCommandDefinition.Flags.Add(Flag);
	}
}

const FCommandDefinition& UConsoleCommandBase::GetCommandDefinition()
{
	if (!CommandDefinition.IsFilled())
	{
		GenerateCommandDefinition(CommandDefinition);
	}

	return CommandDefinition;
}

void UConsoleCommandBase::GenerateCommandDefinition(FCommandDefinition& OutCommandDefinition) const
{
	OutCommandDefinition.Name = this->CommandName;
	OutCommandDefinition.Category = this->Category;
	OutCommandDefinition.Description = this->Description;
	OutCommandDefinition.bServer = this->bIsServerCommand;
	OutCommandDefinition.LifecycleType = this->LifecycleType;
	OutCommandDefinition.bLatent = false;

	FillParameterAndFlagDefinitions(OutCommandDefinition);
}

UWorld* UConsoleCommandBase::GetWorld() const
{
	if (IsTemplate() || !GetOuter())
	{
		return nullptr;
	}
	
	// could be the command is executing and has a valid caller - return its world then
	if (CurrentExecutionContext.Caller.IsValid())
	{
		return CurrentExecutionContext.Caller.Get()->GetWorld();
	}
	
	return GetOuter()->GetWorld();
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

bool UConsoleCommandBase::IsRelevantInCurrentContext_Implementation() const
{
	return true;
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
