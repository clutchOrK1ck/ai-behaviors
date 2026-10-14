#include "Parser.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsoleParserTest, "InGameConsole.ParserTests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FConsoleParserTest::RunTest(const FString& Parameters)
{
	FTokenizer Tokenizer;
	FString ConsoleCommand = "execute true --arg1 10 --arg [ 2 3 5 evtl] --quoted \"Quoted String\"";

	Tokenizer.Parse(ConsoleCommand);

	TestFalse(TEXT("Parsed without errors"), Tokenizer.HasError());
	TestEqual(TEXT("Parsed a correct number of tokens"), Tokenizer.GetTokens().Num(), 8);

	return true;
}



