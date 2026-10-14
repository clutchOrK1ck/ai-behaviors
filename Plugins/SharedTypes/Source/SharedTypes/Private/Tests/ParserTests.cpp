#include "Parser.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParserTests, "UtilityTypes.ParserTests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

using FSimpleValue = TVariant<int32, FString>;

/**
 * a simple parser for use in tests. reads a sequence of either numbers or strings, optionally with double quotes
 */
class FSimpleTestParser : public FParserBase
{
	TArray<FSimpleValue> ParsedValues;

protected:
	
	virtual void ParseImpl() override
	{
		while (Peak() != '\0')
		{
			if (FChar::IsDigit(Peak()))
			{
				ParsedValues.Add(FSimpleValue(TInPlaceType<int32>(), DoReadInt()));
			} else if (Peak() == '"')
			{
				ParsedValues.Add(FSimpleValue(TInPlaceType<FString>(), DoReadQuotedString()));
			} else
			{
				ParsedValues.Add(FSimpleValue(TInPlaceType<FString>(), DoReadNonBlank()));
			}
		}
	}

public:
	const TArray<FSimpleValue>& GetValues() const
	{
		return ParsedValues;
	}
};


bool FParserTests::RunTest(const FString& Parameters)
{
	auto TestParser = FSimpleTestParser();
	const FString TestDescription = "Expect correctly parsed string";

	TestParser.Parse("one 1 \"quoted string\"");

	TestEqual(TestDescription, TestParser.GetValues().Num(), 3);
	
	TestTrue(TestDescription, TestParser.GetValues()[0].IsType<FString>());
	TestEqual(TestDescription, TestParser.GetValues()[0].Get<FString>(), "one");

	TestTrue(TestDescription, TestParser.GetValues()[1].IsType<int32>());
	TestEqual(TestDescription, TestParser.GetValues()[1].Get<int32>(), 1);

	TestTrue(TestDescription, TestParser.GetValues()[2].IsType<FString>());
	TestEqual(TestDescription, TestParser.GetValues()[2].Get<FString>(), "quoted string");
	
	return true;
}

