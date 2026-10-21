// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Commons.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewInput, const FString&);

/**
 * a pipe where messages can be written to. Use for stdout/stderr of console-related types
 */
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

	void Clear()
	{
		Items.Empty();
	}

	const TArray<FString>& GetItems() const
	{
		return Items;
	}
};
