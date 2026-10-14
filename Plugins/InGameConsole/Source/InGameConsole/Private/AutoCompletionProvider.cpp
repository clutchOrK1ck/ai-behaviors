// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoCompletionProvider.h"


bool UAutoCompletionUtils::GetAutoCompletionsBySimpleMatch(const FString& InToAutocomplete,
	const TArray<FString>& Options, TSet<FString>& AutoCompletions)
{
	for (const FString& Option : Options)
	{
		if (Option.Equals(InToAutocomplete))
		{
			AutoCompletions.Add("");
		} else if (Option.Len() > InToAutocomplete.Len() && Option.Mid(0, InToAutocomplete.Len()).Equals(InToAutocomplete))
		{
			AutoCompletions.Add(Option.Mid(InToAutocomplete.Len() - 1));
		}
	}

	return AutoCompletions.Num() > 0;
}
