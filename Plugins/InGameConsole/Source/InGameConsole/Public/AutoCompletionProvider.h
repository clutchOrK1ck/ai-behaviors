// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AutoCompletionProvider.generated.h"


USTRUCT(BlueprintType)
struct FAutocompletion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString Root; // the original word autocompletion has been requested for

	UPROPERTY(BlueprintReadWrite)
	FString SuggestedCompletion; // the full suggested completion (we cannot rely only on suffix because an auto-completion might introduce case changes)

	bool operator==(const FAutocompletion& Other) const
	{
		return Root.Equals(Other.Root, ESearchCase::CaseSensitive)
			&& SuggestedCompletion.Equals(Other.SuggestedCompletion, ESearchCase::CaseSensitive);
	}

	friend uint32 GetTypeHash(const FAutocompletion& Completion)
	{
		return HashCombine(GetTypeHash(Completion.Root), GetTypeHash(Completion.SuggestedCompletion));
	}
};

UINTERFACE(meta=(
	ToolTip = "This interface can be implemented by (latent) console commands that support their own sub-command set"))
class UAutoCompletionProvider : public UInterface
{
	GENERATED_BODY()
};

class INGAMECONSOLE_API IAutoCompletionProvider
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, meta=(
		ToolTip = "Returns autocompletions for a string"))
	bool RequestAutocompletions(const FString& String, TSet<FAutocompletion>& AutoCompletions) const;
};

UCLASS(BlueprintType)
class UAutoCompletionUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=(
		ToolTip =
		"Return autocompletions by comparing the string to auto-complete with the starting characters of each option in the list"
	))
	static bool GetAutoCompletionsBySimpleMatch(const FString& InToAutocomplete, const TArray<FString>& Options,
	                                            TSet<FAutocompletion>& AutoCompletions, bool bIgnoreCase = true);
};
