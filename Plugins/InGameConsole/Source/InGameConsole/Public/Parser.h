#pragma once
#include "ConsoleCommandBase.h"
#include "Parser.h"
#include "SharedTypes/Public/Parser.h"

class FTokenizer : public FParserBase
{
protected:
	TArray<FString> Tokens; // this console command parsed into an array of tokens
	virtual void ParseImpl() override;

public:
	const TArray<FString>& GetTokens() const
	{
		return Tokens;
	}
};

/**
 * a tokenizer to use with autocompletion, allows for unclosed groups and double-quotes
 */
class FAutocompletionTokenizer : public FTokenizer
{
protected:
	virtual void ParseImpl() override;
};

/**
 * this is a utility class for parameter names or flags supplied in the command line (-- and -)
 */
struct FCLIName
{
	FCLIName() : Name(NAME_None), bIsAlias(false) {}
	
	static bool Parse(const FString& Token, FCLIName& OutName)
	{
		
		if (Token.StartsWith(TEXT("--")) && Token.Len() > 2)
		{
			OutName = FCLIName(FName(Token.TrimChar('-').TrimChar('-')), false);
			return true;
		}

		if (Token.StartsWith(TEXT("-")) && Token.Len() > 1)
		{
			OutName = FCLIName(FName(Token.TrimChar('-')), true);
			return true;
		}

		return false;
	}

	bool IsAlias() const
	{
		return bIsAlias;
	}

	const FName& GetName() const
	{
		return Name;
	}

	bool IsNone() const
	{
		return Name.IsNone();
	}
	
private:
	FName Name;
	bool bIsAlias;

	FCLIName(const FName& Name, bool bIsAlias) : Name(Name), bIsAlias(bIsAlias) {}
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
