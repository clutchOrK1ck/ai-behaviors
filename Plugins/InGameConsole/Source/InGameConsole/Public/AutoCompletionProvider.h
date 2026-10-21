// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AutoCompletionProvider.generated.h"


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
	bool RequestAutocompletions(const FString& String, TSet<FString>& AutoCompletions) const;
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
	                                            TSet<FString>& AutoCompletions);
};
