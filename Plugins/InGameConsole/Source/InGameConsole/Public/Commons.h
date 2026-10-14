// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Commons.generated.h"

DECLARE_DELEGATE_OneParam(FOnNewInput, const FString&);

class ITextPipe
{
public:
	virtual void Write(const FString& Message) = 0;
	virtual ~ITextPipe() = default;
};

/**
 * user input to the command
 */
UCLASS()
class UCommandUserInput : public UObject, public ITextPipe
{
	GENERATED_BODY()

	TArray<FString> History;
	
public:
	FOnNewInput OnNewInput;
	virtual void Write(const FString& Item) override
	{
		History.Add(Item);
		OnNewInput.ExecuteIfBound(Item);
	}
};

UCLASS(BlueprintType)
class UConsoleOutputStream : public UObject, public ITextPipe
{
	GENERATED_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMessageWritten, const FString&, Message);

	TArray<FString> Items;
	
public:

	UPROPERTY(BlueprintAssignable)
	FOnMessageWritten OnMessageWrittenDelegate;

	UFUNCTION(BlueprintCallable)
	virtual void Write(const FString& Message) override
	{
		Items.Add(Message);
		OnMessageWrittenDelegate.Broadcast(Message);
	}

	UFUNCTION(BlueprintCallable)
	void Clear()
	{
		Items.Empty();
	}
};
