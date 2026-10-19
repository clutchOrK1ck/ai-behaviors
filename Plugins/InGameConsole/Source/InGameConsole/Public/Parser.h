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
public:
	/**
	 * parse tokens supplied from the CLI to actual arguments, using the command parameter specification
	 * @param Tokens 
	 * @param Command 
	 * @param OutArgs
	 * @param OutError error that occurred while preparing the arguments
	 * @return 
	 */
	bool ParseArguments(const TArray<FString>& Tokens,
		const UConsoleCommandBase* Command,
		FCommandArguments& OutArgs,
		FString& OutError);
};
