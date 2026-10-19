// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Commons.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewInput, const FString&);

class ITextPipe
{
public:
	virtual void Write(const FString& Message) = 0;
	virtual ~ITextPipe() = default;
};

/**
 * a simple text pipe accepting text messages. Emits an event on an incoming text message
 */
UCLASS()
class USimpleTextPipe : public UObject, public ITextPipe
{
	GENERATED_BODY()

	TArray<FString> Items;
	
public:
	FOnNewInput OnNewItem;
	
	virtual void Write(const FString& Message) override
	{
		Items.Add(Message);
		OnNewItem.Broadcast(Message);
	}
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
		OnNewInput.Broadcast(Item);
	}
};

UCLASS(BlueprintType)
class UConsoleOutputStream : public UObject, public ITextPipe
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageWritten, const FString&);

	TArray<FString> Items;
	
public:
	
	FOnMessageWritten OnNewLine;

	UFUNCTION(BlueprintCallable)
	virtual void Write(const FString& Message) override
	{
		Items.Add(Message);
		OnNewLine.Broadcast(Message);
	}

	UFUNCTION(BlueprintCallable)
	void Clear()
	{
		Items.Empty();
	}
};
