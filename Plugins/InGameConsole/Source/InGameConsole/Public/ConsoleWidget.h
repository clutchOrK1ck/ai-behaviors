// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConsoleWidget.generated.h"

UENUM(BlueprintType)
enum class EConsoleLineType : uint8
{
	Display,
	Error
};

/**
 * a console widget. implement this to create an actual widget. basic provided functionality:
 * - intercepts game console's stdOut and stdErr, you only have to implement OnNewLine to handle display
 */
UCLASS()
class INGAMECONSOLE_API UConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintImplementableEvent)
	void OnNewLine(const FString& Line, EConsoleLineType LineType);
};
