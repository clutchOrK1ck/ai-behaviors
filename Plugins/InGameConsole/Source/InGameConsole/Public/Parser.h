#pragma once
#include "ConsoleCommandBase.h"
#include "Parser.h"
#include "SharedTypes/Public/Parser.h"

class FTokenizer : public FParserBase
{
	TArray<FString> Tokens; // this console command parsed into an array of tokens

protected:
	virtual void ParseImpl() override;

public:
	const TArray<FString>& GetTokens() const
	{
		return Tokens;
	}
};

class FCLIParser
{
	FORCEINLINE static bool DoesLookLikeLongArgName(const FString& Token)
	{
		return Token.StartsWith(TEXT("--"));
	}

	FORCEINLINE static bool DoesLookLikeShortArgName(const FString& Token)
	{
		return Token.Len() >= 2 && Token[0] == '-' && Token[1] != '-';
	}

	static bool DoesLookLikeArgName(const FString& Token)
	{
		return DoesLookLikeShortArgName(Token) || DoesLookLikeLongArgName(Token);
	}

	const UParameter* TryFindParameterForToken(const FString& Token, const UConsoleCommandBase* Command) const;
	const FFlagDescription* TryFindFlagSpecForToken(const FString& Token, const UConsoleCommandBase* Command) const;
	
public:
	/**
	 * parse tokens supplied from the CLI to actual arguments, using the command parameter specification
	 * @param InTokens 
	 * @param Command 
	 * @param OutArgs
	 * @param OutError error that occurred while preparing the arguments
	 * @return 
	 */
	bool ParseArguments(const TArray<FString>& InTokens,
		const UConsoleCommandBase* Command,
		FCommandArguments& OutArgs,
		FString& OutError);
};
