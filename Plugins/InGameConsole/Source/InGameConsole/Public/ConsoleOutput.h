#pragma once

#include "CoreMinimal.h"

enum class EConsoleLineType : uint8
{
	Display,
	Error,
	Input
};

struct FConsoleOutputLine
{
	FString Text;
	EConsoleLineType Type;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConsoleOutput, const FConsoleOutputLine&);
DECLARE_MULTICAST_DELEGATE(FOnConsoleOutputCleared);
