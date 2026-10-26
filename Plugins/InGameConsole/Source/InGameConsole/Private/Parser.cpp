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

void FAutocompletionTokenizer::ParseImpl()
{
	Tokens.Empty();

	// as with the normal identifier, a command name or a piece of it is expected at the start
	Tokens.Add(DoReadIdentifier());

	while (Peak() != '\0')
	{
		if (Peak() == '"')
		{
			const uint32 PositionBeforeReadQuotedString = GetCursor();

			try
			{
				Tokens.Add(DoReadQuotedString());
			} catch (FEOL EndOfLine)
			{
				Jump(PositionBeforeReadQuotedString);
				DoReadChar('"');

				Tokens.Add(ReadRemaining().Replace(TEXT("\\\""), TEXT("\"")));
				return;
			}
		} else if (Peak() == '[')
		{
			const uint32 PositionBeforeReadGroup = GetCursor();

			try
			{
				Tokens.Add(
					DoReadEnclosedGroup('[', ']')
					.TrimChar('[')
					.TrimChar(']')
					.TrimStartAndEnd());
			} catch (FEOL EndOfLine)
			{
				Jump(PositionBeforeReadGroup);
				DoReadChar('[');
				Tokens.Add(ReadRemaining());
				return;
			}
		} else
		{
			Tokens.Add(DoReadNonBlank());
		}
	}
}

const UParameter* FCLIParser::TryFindParameterForToken(const FString& Token, const UConsoleCommandBase* Command) const
{
	if (DoesLookLikeLongArgName(Token))
	{
		return Command->FindNamedParamByLongName(FName(Token.TrimChar('-').TrimChar('-')));
	}

	if (DoesLookLikeShortArgName(Token))
	{
		return Command->FindNamedParamByShortName(FName(Token.TrimChar('-')));
	}

	return nullptr;
}

const FFlagDescription* FCLIParser::TryFindFlagSpecForToken(const FString& Token, const UConsoleCommandBase* Command) const
{
	for (const FFlagDescription& FlagSpec : Command->Flags)
	{
		if (DoesLookLikeLongArgName(Token) && FlagSpec.Name.ToString().Equals(Token.TrimChar('-').TrimChar('-')))
		{
			return &FlagSpec;
		}

		if (DoesLookLikeShortArgName(Token) && FlagSpec.ShortName.ToString().Equals(Token.TrimChar('-')))
		{
			return &FlagSpec;
		}
	}

	return nullptr;
}

bool FCLIParser::ParseArguments(const TArray<FString>& InTokens, const UConsoleCommandBase* Command,
                                FCommandArguments& OutArgs, FString& OutError)
{
	auto Tokens = MakeArrayView(InTokens);
	
	// remove the command name
	Tokens = Tokens.RightChop(1);

	// positional parameters
	const int NumPositionalArgs = Command->PositionalParameters.Num();

	for (int i = 0; i < NumPositionalArgs; i++)
	{
		const auto ParamSpec = Command->PositionalParameters[i];
		
		if (!Tokens.IsValidIndex(i))
		{
			OutError = FString::Format(TEXT("Missing positional argument: {0}"), {ParamSpec->Name.ToString()});
			return false;
		}

		if (!InitArgumentValue(ParamSpec, &Tokens[i], OutArgs, Command, OutError))
		{
			return false;
		}
	}

	// consume tokens for the positional arguments
	Tokens = Tokens.RightChop(NumPositionalArgs);

	// the rest of the tokens are named parameters or flags
	for (int i = 0; i < Tokens.Num(); i++)
	{
		const FString Token = Tokens[i];
		
		bool bIsParam = false;
		bool bIsFlag = false;
		const UParameter* ParamSpec;
		const FFlagDescription* FlagSpec = nullptr;

		if (!DoesLookLikeArgName(Token))
		{
			OutError = FString::Format(TEXT("Expected a parameter or flag name, found {0} instead"), {Token});
			return false;
		}

		if (ParamSpec = TryFindParameterForToken(Token, Command); ParamSpec != nullptr)
		{
			bIsParam = true;
		} else if (FlagSpec = TryFindFlagSpecForToken(Token, Command); FlagSpec != nullptr)
		{
			bIsFlag = true;
		} else
		{
			OutError = FString::Format(TEXT("Cannot find either parameter or flag: '{0}'"), {Token});
			return false;
		}
		
		if (bIsParam)
		{
			// you have to supply argument value if you use its name in the command
			if (!Tokens.IsValidIndex(++i))
			{
				OutError = FString::Format(TEXT("Expected a value for parameter: {0}"), {Token});
				return false;
			}

			if (!InitArgumentValue(ParamSpec, &Tokens[i], OutArgs, Command, OutError))
			{
				return false;
			}
		} else if (bIsFlag)
		{
			OutArgs.AddFlag(FlagSpec->Name.ToString());
		}
	}

	return Command->ValidateArguments(OutArgs, OutError);
}
