// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AutoCompletionProvider.h"
#include "UObject/Object.h"
#include "CommandParameters.generated.h"

class UConsoleCommandBase;

/**
 * only six accepted argument types
 * - whole numbers (int32) and floating point (double)
 * - bool (can be supplied as yes/y/true/false/no/n
 * - strings, optionally enclosed in double quotes
 * - lists (TArray<FString>), members are enclosed in square brackets and separated with a space
 * - FVector, vectors must be supplied as three numeric values enclosed in square brackets and separated with a space
 */
using FArgumentValue = TVariant<int32, double, bool, FString, FVector, TArray<FString>>;

/**
 * a container for the arguments supplied to the command, sendable over the network
 */
USTRUCT()
struct FCommandArguments
{
	GENERATED_BODY()
	
	template<typename T>
	bool Get(const FName& Name, T& OutValue) const;
	
	template<typename T>
	void Add(const FName& ArgName, const T& ArgValue);

	void AddFlag(const FString& Flag)
	{
		Flags.Add(Flag);
	}

	bool HasFlag(const FString& Flag) const
	{
		return Flags.Contains(Flag);
	}

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

private:
	TMap<FName, FArgumentValue> Args;
	TSet<FString> Flags;
};

template<>
struct TStructOpsTypeTraits<FCommandArguments> : public TStructOpsTypeTraitsBase2<FCommandArguments>
{
	enum
	{
		WithNetSerializer = true
	};
};

template <typename T>
bool FCommandArguments::Get(const FName& Name, T& OutValue) const
{
	if (Args.Contains(Name) && Args[Name].IsType<T>())
	{
		OutValue = Args[Name].Get<T>();
		return true;
	}

	return false;
}

template <typename T>
void FCommandArguments::Add(const FName& ArgName, const T& ArgValue)
{
	Args.Add(ArgName, FArgumentValue{TInPlaceType<T>(), ArgValue});
}

template<typename T>
class TConsoleParameter
{
public:
	virtual FName GetParamName() const = 0;
	virtual bool ParseAndValidate(const FString& Value, const UConsoleCommandBase* Command, T& OutValue, FString& ErrorMessage) const = 0;
	virtual T GetDefaultValue() const = 0;
	virtual bool IsRequired() const = 0;
	virtual ~TConsoleParameter() = default;
};

UCLASS(BlueprintType, Abstract, CollapseCategories)
class UParameter : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(
		ToolTip =
		"This argument's name. Note: also for positional arguments, you must specify the name and you access them by the name. Argument being positional only affects CLI parsing"
	))
	FName Name;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(
		ToolTip = "Short name, when prefixed with '-' instead of '--"))
	FName ShortName = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRequired;
};

UCLASS(BlueprintType, EditInlineNew)
class UIntegerParameter : public UParameter, public TConsoleParameter<int32>
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(EditCondition = "!bRequired", EditConditionHides, DisplayAfter = "bRequired"))
	int32 DefaultValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter")
	bool bRestrictRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(EditCondition = "bRestrictRange"))
	int32 MinValue = MIN_int32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(EditCondition = "bRestrictRange"))
	int32 MaxValue = MAX_int32;
	
	virtual bool ParseAndValidate(const FString& Value, const UConsoleCommandBase* Command, int32& OutValue, FString& ErrorMessage) const override;

	virtual int32 GetDefaultValue() const override
	{
		return DefaultValue;
	}

	virtual bool IsRequired() const override
	{
		return bRequired;
	}

	virtual FName GetParamName() const override
	{
		return Name;
	}
};

UENUM(BlueprintType)
enum EStringValidationType
{
	None,
	AllowedValueSet,
	Regex
};

USTRUCT(BlueprintType)
struct FStringValidationSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter")
	TEnumAsByte<EStringValidationType> ValidationType = None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(
		ToolTip = "If true, allowed values for this parameter will be returned by 'GetAllowedValues' method of the console command",
		EditCondition = "ValidationType == EStringValidationType::AllowedValueSet",
		EditConditionHides))
	bool bProvideAllowedValuesDynamically;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(
		EditCondition = "ValidationType == EStringValidationType::AllowedValueSet && !bProvideAllowedValuesDynamically",
		EditConditionHides))
	TSet<FString> AllowedValues;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(
		EditCondition = "ValidationType == EStringValidationType::Regex",
		EditConditionHides))
	FString Regex;

	bool Validate(const FString& Value, FString& OutError) const;
};

UCLASS(BlueprintType, EditInlineNew)
class UStringParameter : public UParameter, public TConsoleParameter<FString>, public IAutoCompletionProvider
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(EditCondition = "!bRequired", EditConditionHides, DisplayAfter = "bRequired"))
	FString DefaultValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Validation")
	FStringValidationSettings Validation;
	
	virtual bool ParseAndValidate(const FString& Value, const UConsoleCommandBase* Command, FString& OutValue, FString& ErrorMessage) const override;

	virtual FString GetDefaultValue() const override
	{
		return DefaultValue;
	}

	virtual bool IsRequired() const override
	{
		return bRequired;
	}

	virtual FName GetParamName() const override
	{
		return Name;
	}

	virtual bool RequestAutocompletions_Implementation(const FString& String, TSet<FString>& AutoCompletions) const override;
};

UCLASS(BlueprintType, EditInlineNew)
class UListParameter : public UParameter, public TConsoleParameter<TArray<FString>>, public IAutoCompletionProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(EditCondition = "!bRequired", EditConditionHides, DisplayAfter = "bRequired"))
	TArray<FString> DefaultValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Validation", meta=(
		ToolTip = "Validation will be applied to each member of the list"))
	FStringValidationSettings Validation;
	
	virtual bool ParseAndValidate(const FString& Value, const UConsoleCommandBase* Command, TArray<FString>& OutValue, FString& ErrorMessage) const override;

	virtual TArray<FString> GetDefaultValue() const override
	{
		return DefaultValue;
	}

	virtual bool IsRequired() const override
	{
		return bRequired;
	}

	virtual FName GetParamName() const override
	{
		return Name;
	}

	virtual bool RequestAutocompletions_Implementation(const FString& String, TSet<FString>& AutoCompletions) const override;
};

UCLASS(BlueprintType, EditInlineNew)
class UVectorParameter : public UParameter, public TConsoleParameter<FVector>
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector DefaultValue;
	
	virtual bool ParseAndValidate(const FString& Value, const UConsoleCommandBase* Command, FVector& OutValue, FString& ErrorMessage) const override;

	virtual FVector GetDefaultValue() const override
	{
		return DefaultValue;
	}

	virtual bool IsRequired() const override
	{
		return bRequired;
	}

	virtual FName GetParamName() const override
	{
		return Name;
	}
};

UCLASS(BlueprintType, EditInlineNew)
class UBoolParameter : public UParameter, public TConsoleParameter<bool>
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool DefaultValue;
	
	virtual bool ParseAndValidate(const FString& Value, const UConsoleCommandBase* Command, bool& OutValue, FString& ErrorMessage) const override;

	virtual bool GetDefaultValue() const override
	{
		return DefaultValue;
	}

	virtual bool IsRequired() const override
	{
		return bRequired;
	}

	virtual FName GetParamName() const override
	{
		return Name;
	}
};

UCLASS(BlueprintType, EditInlineNew)
class URealParameter : public UParameter, public TConsoleParameter<double>
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	double DefaultValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter")
	bool bRestrictRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(EditCondition = "bRestrictRange"))
	double MinValue = MIN_dbl;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parameter", meta=(EditCondition = "bRestrictRange"))
	double MaxValue = MAX_dbl;

	//~ Begin TConsoleParameter interface
	virtual FName GetParamName() const override
	{
		return Name;
	}
	
	virtual bool ParseAndValidate(const FString& Value, const UConsoleCommandBase* Command, double& OutValue,
		FString& ErrorMessage) const override;
	
	virtual double GetDefaultValue() const override
	{
		return DefaultValue;
	}
	
	virtual bool IsRequired() const override
	{
		return bRequired;
	}
	//~ End TConsoleParameter interface
};

USTRUCT(BlueprintType)
struct FFlagDescription
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName Name;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ShortName = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString Description;

	friend uint32 GetTypeHash(const FFlagDescription& FlagDescription)
	{
		return GetTypeHash(FlagDescription.Name); // TODO this ignores short name completely
	}

	bool operator==(const FFlagDescription& Other) const
	{
		return Other.Name == Name; // TODO see above
	}
};

template<typename T>
bool InitArgumentValue(const TConsoleParameter<T>* ParamSpec,  const FString* Value, FCommandArguments& OutArgs, const UConsoleCommandBase* Command, FString& OutError)
{
	if (Value == nullptr)
	{
		if (ParamSpec->IsRequired())
		{
			OutError = FString::Format(TEXT("{0} is required but no value was supplied"), {ParamSpec->GetParamName().ToString()});
			return false;
		}

		OutArgs.Add(ParamSpec->GetDefaultValue());
		return true;
	}

	T ParsedValue;
	if (!ParamSpec->ParseAndValidate(*Value, Command, ParsedValue, OutError))
	{
		return false;
	}

	OutArgs.Add(ParsedValue);
	return true;
}

inline bool InitArgumentValue(const UParameter* ParamSpec, const FString* Value, FCommandArguments& OutArgs, const UConsoleCommandBase* Command, FString& OutError)
{
	if (const auto* IntParam = Cast<UIntegerParameter>(ParamSpec))
	{
		return InitArgumentValue(IntParam, Value, OutArgs, Command, OutError);
	}

	if (const auto* BoolParam = Cast<UBoolParameter>(ParamSpec))
	{
		return InitArgumentValue(BoolParam, Value, OutArgs, Command, OutError);
	}

	if (const auto* StringParam = Cast<UStringParameter>(ParamSpec))
	{
		return InitArgumentValue(StringParam, Value, OutArgs, Command, OutError);
	}

	if (const auto* ListParam = Cast<UListParameter>(ParamSpec))
	{
		return InitArgumentValue(ListParam, Value, OutArgs, Command, OutError);
	}

	if (const auto* VectorParam = Cast<UVectorParameter>(ParamSpec))
	{
		return InitArgumentValue(VectorParam, Value, OutArgs, Command, OutError);
	}

	return false;
}

template<typename T>
bool CheckInRange(const T& CheckValue, const T& ValueMin, const T& ValueMax)
{
	return CheckValue >= ValueMin && CheckValue <= ValueMax;
}
