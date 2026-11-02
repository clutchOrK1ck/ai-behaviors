// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoCompletionProvider.h"


bool UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(const FString& InToAutocomplete,
                                                           const TArray<FString>& Options,
                                                           TSet<FAutocompletion>& AutoCompletions, bool bIgnoreCase)
{
	ESearchCase::Type SearchCase = bIgnoreCase ? ESearchCase::IgnoreCase : ESearchCase::CaseSensitive;

	for (const FString& Option : Options)
	{
		if (
			Option.Equals(InToAutocomplete, SearchCase) ||
			(Option.Len() > InToAutocomplete.Len() && Option.Mid(0, InToAutocomplete.Len()).Equals(
				InToAutocomplete, SearchCase))
		)
		{
			AutoCompletions.Add(FAutocompletion(InToAutocomplete, Option));
		}
	}

	return AutoCompletions.Num() > 0;
}
