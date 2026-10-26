#pragma once

#include "CoreMinimal.h"
#include "ConsoleOutput.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Views/SListView.h"

class UGameConsole;

class SConsoleEditableText : public SEditableText
{
public:

	int32 Caret() const;

	float CaretX() const;
};

class SScrollBox;
class SVerticalBox;

/** View and keyboard interaction only; command parsing/execution remains in UGameConsole. */
class SGameConsole : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SGameConsole) {}
		SLATE_ARGUMENT(UGameConsole*, Console)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual ~SGameConsole() override;

	void FocusInput();

	void DismissCompletions();

	virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	virtual void Tick(const FGeometry& Geometry, double Time, float DeltaTime) override;

private:

	friend class FGameConsoleSlateTest;

	TWeakObjectPtr<UGameConsole> Console;
	TSharedPtr<SConsoleEditableText> Input;
	TSharedPtr<SScrollBox> OutputScroll;
	TSharedPtr<SVerticalBox> OutputBox;
	TSharedPtr<SListView<TSharedPtr<FString>>> CompletionList;

	TArray<TSharedPtr<FString>> Completions;
	FString CompletionSource;
	int32 CompletionCaret = 0;
	int32 SelectedCompletion = 0;
	int32 HistoryOffset = 0;
	FString HistoryDraft;

	bool bProgrammaticEdit = false;
	FVector2D PopupPosition = FVector2D::ZeroVector;
	FVector2D PopupSize = FVector2D(420, 180);

	FDelegateHandle OutputHandle;
	FDelegateHandle ClearHandle;

	void AppendOutput(const FConsoleOutputLine& Line);

	void ClearOutput();

	void InputChanged(const FText& Text);

	void InputCommitted(const FText& Text, ETextCommit::Type CommitType);

	FReply InputCharacter(const FGeometry& Geometry, const FCharacterEvent& Event);

	void SetInput(const FString& Text, int32 Caret);

	void BrowseHistory(int32 Direction);

	void RequestCompletions();

	void AcceptCompletion();

	void MoveCompletion(int32 Direction);

	TSharedRef<ITableRow> MakeCompletionRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner);

};
