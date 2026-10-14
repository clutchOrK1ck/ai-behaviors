// Fill out your copyright notice in the Description page of Project Settings.


#include "CommandParameters.h"
#include "ConsoleCommand.h"

bool UIntegerParameter::ParseAndValidate(const FString& Value, const UConsoleCommand* Command, int32& OutValue,
	FString& ErrorMessage) const
{
	if (!LexTryParseString(OutValue, *Value))
	{
		ErrorMessage = "Expected an integer";
		return false;
	}

	return true;
}

bool FStringValidationSettings::Validate(const FString& Value, FString& OutError) const
{
	switch (ValidationType)
	{
	case None:
		return true;
	case EStringValidationType::Regex:
		{
			// TODO how do we make sure the regex pattern itself is valid?
			const FRegexPattern Pattern{Regex};
			FRegexMatcher Matcher{Pattern, Value};

			if (!Matcher.FindNext())
			{
				OutError = FString::Format(TEXT("String argument {0} does not satisfy regex: {1}"), {Value, Regex});
				return false;
			}
			
			return true;
		}
	case AllowedValueSet:
		if (bProvideAllowedValuesDynamically)
		{
			return false; // validated on a higher level (see validation in string and list params)
		}

		if (!AllowedValues.Contains(Value))
		{
			OutError = FString::Format(TEXT("String argument {0} is not in the list of allowed values"), {Value});
			return false;
		}
		
		return true;
	default:
		return false;
	}
}

bool UStringParameter::ParseAndValidate(const FString& Value, const UConsoleCommand* Command, FString& OutValue,
                                        FString& ErrorMessage) const
{
	if (Validation.bProvideAllowedValuesDynamically)
	{
		if (!Command->CheckParamValueIsInAllowedSet(Name, Value))
		{
			ErrorMessage = FString::Format(TEXT("{0} is not in the set of allowed values"), {Name.ToString()});
			return false;
		}
		
		return true;
	}
	
	if (!Validation.Validate(Value, ErrorMessage))
	{
		return false;
	}

	OutValue = Value;
	return true;
}

bool UStringParameter::RequestAutocompletions_Implementation(const FString& String, TSet<FString>& AutoCompletions) const
{
	// auto-completing a string value only makes sense if values are restricted to a set
	if (Validation.ValidationType == AllowedValueSet && !Validation.bProvideAllowedValuesDynamically)
	{
		return UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(String, Validation.AllowedValues.Array(), AutoCompletions);
	}

	// TODO how do we go about dynamic value sets?
	return false;
}

bool UListParameter::RequestAutocompletions_Implementation(const FString& String, TSet<FString>& AutoCompletions) const
{
	if (Validation.ValidationType == AllowedValueSet && !Validation.bProvideAllowedValuesDynamically)
	{
		// try to autocomplete the last array member
		TArray<FString> ListMembers;
		String.ParseIntoArray(ListMembers, TEXT(" "), true);

		if (ListMembers.Num() > 0)
		{
			return UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(ListMembers.Last(), Validation.AllowedValues.Array(), AutoCompletions);
		}
	}

	// TODO dynamic value sets
	return false;
}

bool UListParameter::ParseAndValidate(const FString& Value, const UConsoleCommand* Command, TArray<FString>& OutValue,
                                      FString& ErrorMessage) const
{
	// if not already done by the parser, remove the list brackets and the trailing spaces
	const auto Val = Value.TrimChar('[').TrimChar(']').TrimStartAndEnd();

	// split by spaces
	TArray<FString> Members;
	Val.ParseIntoArray(Members, TEXT(" "), true);
	
	for (const FString& Member : Members)
	{
		if (Validation.bProvideAllowedValuesDynamically)
		{
			if (!Command->CheckParamValueIsInAllowedSet(Name, Value))
			{
				ErrorMessage = FString::Format(TEXT("{0} is not in the set of allowed values"), {Name.ToString()});
				return false;
			}
		
			return true;
		}
		
		if (!Validation.Validate(Member, ErrorMessage))
		{
			return false;
		}

		OutValue.Add(Member);
	}

	return true;
}

bool UVectorParameter::ParseAndValidate(const FString& Value, const UConsoleCommand* Command, FVector& OutValue,
	FString& ErrorMessage) const
{
	// remove square brackets (parser should have taken care, just in case)
	const FString Val = Value.TrimChar('[').TrimChar(']').TrimStartAndEnd();

	// try to parse exactly three numeric values
	TArray<FString> Parts;
	Value.ParseIntoArray(Parts, TEXT(" "), true);

	if (Parts.Num() != 3)
	{
		ErrorMessage = "Expected a list of length = 3";
		return false;
	}
	
	for (int i = 0; i < Parts.Num(); i++)
	{
		double DoubleVal;

		if (!LexTryParseString(DoubleVal, *Parts[i]))
		{
			ErrorMessage = FString::Format(TEXT("Expected numeric at position {0}"), {i});
			return false;
		}
		
		OutValue[i] = DoubleVal;
	}

	return true;
}

bool UBoolParameter::ParseAndValidate(const FString& Value, const UConsoleCommand* Command, bool& OutValue,
                                      FString& ErrorMessage) const
{
	if (Value.IsEmpty())
	{
		return false;
	}

	if (const FString Val = Value.TrimStartAndEnd().ToLower(); TArray<FString>({"yes", "true", "y"}).Contains(Val))
	{
		OutValue = true;
		return true;
	} else if (TArray<FString>({"no", "false", "n"}).Contains(Val))
	{
		OutValue = false;
		return true;
	}

	return false;
}
