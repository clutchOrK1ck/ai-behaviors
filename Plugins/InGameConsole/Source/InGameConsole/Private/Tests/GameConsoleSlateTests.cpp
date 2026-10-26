#include "Widgets/SGameConsole.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "GameConsole.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SBoxPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameConsoleSlateTest, "InGameConsole.Slate.TerminalInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameConsoleSlateTest::RunTest(const FString&)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate must be initialized for the terminal interaction test."));
		return false;
	}

	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UGameConsole> Console(NewObject<UGameConsole>(Instance.Get()));
	FSubsystemCollection<UGameInstanceSubsystem> Collection;
	Console->Initialize(Collection);

	// Output received before the view exists must retain cross-stream order and severity.
	Console->GetMutableStdOut()->Write(TEXT("first"));
	Console->GetMutableStdErr()->Write(TEXT("second"));
	TestEqual(TEXT("Both streams are buffered"), Console->GetOutputLines().Num(), 2);
	TestEqual(TEXT("Standard output comes first"), Console->GetOutputLines()[0].Text, FString(TEXT("first")));
	TestTrue(TEXT("Standard error retains its severity"), Console->GetOutputLines()[1].Type == EConsoleLineType::Error);
	TSharedRef<SGameConsole> View = SNew(SGameConsole).Console(Console.Get());
	TestEqual(TEXT("New view replays the buffer"), View->OutputBox->GetChildren()->Num(), 2);
	Console->GetMutableStdOut()->Write(TEXT("third"));
	TestEqual(TEXT("Live output is appended once"), View->OutputBox->GetChildren()->Num(), 3);

	View->SetInput(TEXT("draft"), 5);
	View->BrowseHistory(-1);
	TestEqual(TEXT("Down at the newest entry preserves the draft"), View->Input->GetText().ToString(), FString(TEXT("draft")));
	View->BrowseHistory(1);
	TestEqual(TEXT("Empty history preserves the draft"), View->Input->GetText().ToString(), FString(TEXT("draft")));

	// Inserting a completion must retain the text to the right of the caret.
	View->SetInput(TEXT("he --count 2"), 2);
	View->CompletionSource = TEXT("he --count 2");
	View->CompletionCaret = 2;
	View->Completions.Add(MakeShared<FString>(TEXT("lp")));
	View->SelectedCompletion = 0;
	View->AcceptCompletion();
	TestEqual(TEXT("Completion inserts at the caret"), View->Input->GetText().ToString(), FString(TEXT("help --count 2")));
	TestEqual(TEXT("Caret follows the insertion"), View->Input->Caret(), 4);
	TestTrue(TEXT("Accepted completion dismisses the popup"), View->Completions.IsEmpty());

	View->Completions.Add(MakeShared<FString>(TEXT("one")));
	View->Completions.Add(MakeShared<FString>(TEXT("two")));
	View->SelectedCompletion = 0;
	View->MoveCompletion(-1);
	TestEqual(TEXT("Up wraps to the last suggestion"), View->SelectedCompletion, 1);
	View->MoveCompletion(1);
	TestEqual(TEXT("Down wraps to the first suggestion"), View->SelectedCompletion, 0);
	View->DismissCompletions();

	Console->Reset();
	TestTrue(TEXT("Reset clears buffered output"), Console->GetOutputLines().IsEmpty());
	TestEqual(TEXT("Reset clears the rendered output"), View->OutputBox->GetChildren()->Num(), 0);
	Console->Deinitialize();
	return true;
}
#endif
