#include "Parser.h"

void FTokenizer::ParseImpl()
{
	Tokens.Empty();

	// the command name
	Tokens.Add(DoReadIdentifier());

	// read everything else respecting the quoted strings and spaces
	while (Peak() != '\0')
	{
		if (Peak() == '"')
		{
			Tokens.Add(DoReadQuotedString());
		} else if (Peak() == '[')
		{
			FString ListGroup = DoReadEnclosedGroup('[', ']');
			Tokens.Add(ListGroup.TrimChar('[').TrimChar(']').TrimStartAndEnd());
		} else
		{
			Tokens.Add(DoReadNonBlank());
		}
	}
}

bool FCLIParser::ParseArguments(const TArray<FString>& Tokens, const UConsoleCommandBase* Command,
                                FCommandArguments& OutArgs, FString& OutError)
{
	// -- positional parameters --
	for (int PositionalParamIdx = 0; PositionalParamIdx < Command->PositionalParameters.Num(); PositionalParamIdx++)
	{
		const FString* SuppliedValuePtr = nullptr;
		if (Tokens.IsValidIndex(PositionalParamIdx + 1)) // we have the command name at idx = 0
		{
			SuppliedValuePtr = &Tokens[PositionalParamIdx + 1];
		}

		const auto ParameterSpec = Command->PositionalParameters[PositionalParamIdx];
		if (!InitArgumentValue(ParameterSpec, SuppliedValuePtr, OutArgs, Command, OutError))
		{
			return false;
		}
	}

	// -- named parameters --
	for (const auto ParamSpec : Command->NamedParameters)
	{
		// try to find by long name first
		int TokenIdx = Tokens.IndexOfByPredicate([ParamSpec](const FString& Token)
		{
			return Token == FString("--") + ParamSpec->Name.ToString();
		});

		// if failed, try to find by short name
		if (TokenIdx <= 0)
		{
			TokenIdx = Tokens.IndexOfByPredicate([ParamSpec](const FString& Token)
			{
				return Token == FString("-" + ParamSpec->ShortName.ToString());
			});
		}

		const bool bFound = TokenIdx > 0; // command name at 0

		if (!bFound)
		{
			// try to initialize with its default value (this will fail if the param is required)
			if (!InitArgumentValue(ParamSpec, nullptr, OutArgs, Command, OutError))
			{
				return false;
			}

			continue;
		}

		// supplied the name - supply the value
		if (!Tokens.IsValidIndex(TokenIdx + 1))
		{
			OutError = FString::Format(TEXT("Expected a value supplied for parameter '{0}'"), {ParamSpec->Name.ToString()});
			return false;
		}

		if (!InitArgumentValue(ParamSpec, &Tokens[TokenIdx + 1], OutArgs, Command, OutError))
		{
			return false;
		}
	}

	// flags
	for (const auto& FlagSpec : Command->Flags)
	{
		// try to find by long name and by short name
		if (Tokens.ContainsByPredicate([&FlagSpec](const FString& Token)
		{
			return Token == FString("--") + FlagSpec.Name.ToString() ||
				Token == FString("-") + FlagSpec.ShortName.ToString();
		}))
		{
			OutArgs.AddFlag(FlagSpec.Name.ToString());
		}
	}

	return true;
}
