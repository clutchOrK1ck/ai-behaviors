#include "Widgets/SGameConsole.h"

#include "GameConsole.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SlateEditableTextLayout.h"
#include "Widgets/Views/STableRow.h"

namespace
{
	FSlateFontInfo TerminalFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Mono", 14);
	}
}

/** Expose Slate's existing cursor/scroll state rather than reimplementing text editing. */
int32 SConsoleEditableText::Caret() const
{
	return EditableTextLayout->GetCursorLocation().GetOffset();
}

float SConsoleEditableText::CaretX() const
{
	const FString Prefix = GetText().ToString().Left(Caret());
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Prefix, TerminalFont()).X
		- FVector2D(EditableTextLayout->GetScrollOffset()).X;
}

void SGameConsole::Construct(const FArguments& InArgs)
{
	Console = InArgs._Console;
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.Padding(32)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get()
			.GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.018f, 0.022f, 0.03f, 0.93f))
			.Padding(18)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 12)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("CONSOLE")))
					.Font(TerminalFont())
					.ColorAndOpacity(FLinearColor(0.5f, 0.75f, 0.85f))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SAssignNew(OutputScroll, SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(OutputBox, SVerticalBox)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 12, 0, 0)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get()
					.GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.07f, 0.085f, 0.105f))
					.Padding(10)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0, 0, 10, 0)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT(">")))
							.Font(TerminalFont())
							.ColorAndOpacity(FLinearColor(0.5f, 0.85f, 0.65f))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1)
						[
							SAssignNew(Input, SConsoleEditableText)
							.Font(TerminalFont())
							.ColorAndOpacity(FLinearColor(0.9f, 0.92f, 0.95f))
							.HintText(FText::FromString(TEXT("Enter a command...")))
							.SelectAllTextWhenFocused(false)
							.SelectAllTextOnCommit(false)
							.ClearKeyboardFocusOnCommit(false)
							.RevertTextOnEscape(false)
							.OnTextChanged(this, &SGameConsole::InputChanged)
							.OnTextCommitted(this, &SGameConsole::InputCommitted)
							.OnKeyCharHandler(this, &SGameConsole::InputCharacter)
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Tab  complete     Up/Down  history     PgUp/PgDn  scroll     Esc  close")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(FLinearColor(0.45f, 0.5f, 0.57f))
				]
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SCanvas)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+ SCanvas::Slot()
			.Position_Lambda([this] { return PopupPosition; })
			.Size_Lambda([this] { return PopupSize; })
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get()
				.GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.09f, 0.12f, 0.16f))
				.Padding(4)
				.Visibility_Lambda([this] { return Completions.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				[
					SAssignNew(CompletionList, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&Completions)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SGameConsole::MakeCompletionRow)
					.OnMouseButtonClick_Lambda([this](TSharedPtr<FString> Item)
					{
						SelectedCompletion = Completions.IndexOfByKey(Item);
						AcceptCompletion();
					})
				]
			]
		]
	];
	if (UGameConsole* Owner = Console.Get())
	{
		for (const FConsoleOutputLine& Line : Owner->GetOutputLines())
		{
			AppendOutput(Line);
		}

		OutputHandle = Owner->OnOutput.AddSP(this, &SGameConsole::AppendOutput);
		ClearHandle = Owner->OnOutputCleared.AddSP(this, &SGameConsole::ClearOutput);
	}
}

SGameConsole::~SGameConsole()
{
	if (UGameConsole* Owner = Console.Get())
	{
		Owner->OnOutput.Remove(OutputHandle);
		Owner->OnOutputCleared.Remove(ClearHandle);
	}
}

void SGameConsole::FocusInput()
{
	FSlateApplication::Get().SetKeyboardFocus(Input, EFocusCause::SetDirectly);
}

void SGameConsole::AppendOutput(const FConsoleOutputLine& Line)
{
	const bool bFollow = OutputScroll->GetScrollOffset() >= OutputScroll->GetScrollOffsetOfEnd() - 2.f;
	const FLinearColor Color = Line.Type == EConsoleLineType::Error ? FLinearColor(1.f, 0.4f, 0.4f)
		: Line.Type == EConsoleLineType::Input ? FLinearColor(0.5f, 0.85f, 0.65f) : FLinearColor(0.85f, 0.88f, 0.92f);
	OutputBox->AddSlot().AutoHeight().Padding(0, 2)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Line.Text))
		.Font(TerminalFont())
		.ColorAndOpacity(Color)
		.AutoWrapText(true)
	];
	if (bFollow)
	{
		OutputScroll->ScrollToEnd();
	}
}

void SGameConsole::ClearOutput()
{
	OutputBox->ClearChildren();
	OutputScroll->SetScrollOffset(0);
	HistoryOffset = 0;
	HistoryDraft.Empty();
	DismissCompletions();
}

void SGameConsole::InputChanged(const FText& Text)
{
	DismissCompletions();
	if (!bProgrammaticEdit)
	{
		HistoryOffset = 0;
	}
}

void SGameConsole::SetInput(const FString& Text, int32 Caret)
{
	TGuardValue<bool> Guard(bProgrammaticEdit, true);
	Input->SetText(FText::FromString(Text));
	Input->GoTo(FTextLocation(0, FMath::Clamp(Caret, 0, Text.Len())));
}

void SGameConsole::InputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter)
	{
		return;
	}

	const FString Command = Text.ToString();
	SetInput(TEXT(""), 0);
	HistoryOffset = 0;
	HistoryDraft.Empty();
	OutputScroll->ScrollToEnd();
	if (UGameConsole* Owner = Console.Get())
	{
		Owner->ProcessConsoleInput(Command);
	}
}

FReply SGameConsole::InputCharacter(const FGeometry&, const FCharacterEvent& Event)
{
	if (const UGameConsole* Owner = Console.Get(); Owner && Owner->bToggleKeyPressed)
	{
		return FReply::Handled();
	}

	// Tab and Enter are handled as keys/commits, never inserted as text.
	return Event.GetCharacter() == '\t' ? FReply::Handled() : FReply::Unhandled();
}

void SGameConsole::BrowseHistory(int32 Direction)
{
	UGameConsole* Owner = Console.Get();
	if (!Owner)
	{
		return;
	}

	if (HistoryOffset == 0 && Direction < 0)
	{
		return;
	}

	if (HistoryOffset == 0 && Direction > 0)
	{
		HistoryDraft = Input->GetText().ToString();
	}

	const int32 NextOffset = FMath::Max(0, HistoryOffset + Direction);
	FString Text;
	if (NextOffset == 0)
	{
		Text = HistoryDraft;
	} else if (!Owner->LookUpExecutionHistoryAtOffset(NextOffset, Text))
	{
		return;
	}

	HistoryOffset = NextOffset;
	SetInput(Text, Text.Len());
}

void SGameConsole::RequestCompletions()
{
	UGameConsole* Owner = Console.Get();
	if (!Owner || Input->AnyTextSelected())
	{
		return;
	}

	CompletionSource = Input->GetText().ToString();
	CompletionCaret = Input->Caret();
	TSet<FString> Suggestions;
	Owner->SuggestAutoCompletions(CompletionSource, CompletionCaret, Suggestions);
	Completions.Empty();
	TArray<FString> Sorted = Suggestions.Array();
	Sorted.Sort();
	for (const FString& Suffix : Sorted)
	{
		if (!Suffix.IsEmpty())
		{
			Completions.Add(MakeShared<FString>(Suffix));
		}
	}

	SelectedCompletion = 0;
	CompletionList->RequestListRefresh();
	if (!Completions.IsEmpty())
	{
		CompletionList->SetSelection(Completions[0]);
		CompletionList->RequestScrollIntoView(Completions[0]);
	}
}

void SGameConsole::DismissCompletions()
{
	Completions.Empty();
	if (CompletionList.IsValid())
	{
		CompletionList->RequestListRefresh();
	}
}

void SGameConsole::MoveCompletion(int32 Direction)
{
	SelectedCompletion = (SelectedCompletion + Direction + Completions.Num()) % Completions.Num();
	CompletionList->SetSelection(Completions[SelectedCompletion]);
	CompletionList->RequestScrollIntoView(Completions[SelectedCompletion]);
}

void SGameConsole::AcceptCompletion()
{
	if (!Completions.IsValidIndex(SelectedCompletion))
	{
		return;
	}

	const FString Suffix = *Completions[SelectedCompletion];
	const FString Text = CompletionSource.Left(CompletionCaret) + Suffix + CompletionSource.Mid(CompletionCaret);
	SetInput(Text, CompletionCaret + Suffix.Len());
	DismissCompletions();
	FocusInput();
}

TSharedRef<ITableRow> SGameConsole::MakeCompletionRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FString>>, Owner)
	.Padding(6)
	[
		SNew(STextBlock)
		.Text(FText::FromString(CompletionSource.Left(CompletionCaret) + *Item))
		.Font(TerminalFont())
		.ColorAndOpacity(FLinearColor::White)
	];
}

FReply SGameConsole::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == GetDefault<UGameConsoleSettings>()->ToggleKey || (Key == EKeys::Escape && Completions.IsEmpty()))
	{
		if (UGameConsole* Owner = Console.Get())
		{
			Owner->HideUI();
		}

		return FReply::Handled();
	}

	if (Key == EKeys::Escape)
	{
		DismissCompletions();
		FocusInput();
		return FReply::Handled();
	}

	if (Key == EKeys::Tab)
	{
		if (Completions.IsEmpty())
		{
			RequestCompletions();
		} else
		{
			MoveCompletion(Event.IsShiftDown() ? -1 : 1);
		}

		return FReply::Handled();
	}

	if (Key == EKeys::Enter && !Completions.IsEmpty())
	{
		AcceptCompletion();
		return FReply::Handled();
	}

	if (Key == EKeys::Up || Key == EKeys::Down)
	{
		if (!Completions.IsEmpty())
		{
			MoveCompletion(Key == EKeys::Up ? -1 : 1);
		} else
		{
			BrowseHistory(Key == EKeys::Up ? 1 : -1);
		}

		return FReply::Handled();
	}

	if (Key == EKeys::PageUp || Key == EKeys::PageDown)
	{
		const float Page = FMath::Max(64.f, OutputScroll->GetCachedGeometry().GetLocalSize().Y * 0.9f);
		OutputScroll->SetScrollOffset(FMath::Clamp(OutputScroll->GetScrollOffset()
			+ (Key == EKeys::PageUp ? -Page : Page), 0.f, OutputScroll->GetScrollOffsetOfEnd()));
		return FReply::Handled();
	}

	DismissCompletions();
	return FReply::Unhandled();
}

FReply SGameConsole::OnKeyDown(const FGeometry&, const FKeyEvent&)
{
	// Consume unhandled keys while the full-screen console is open.
	return FReply::Handled();
}

void SGameConsole::Tick(const FGeometry& Geometry, double Time, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, Time, DeltaTime);
	if (Completions.IsEmpty())
	{
		return;
	}

	if (Input->GetText().ToString() != CompletionSource || Input->Caret() != CompletionCaret)
	{
		DismissCompletions();
		return;
	}

	const FGeometry& InputGeometry = Input->GetCachedGeometry();
	const FVector2D Anchor = Geometry.AbsoluteToLocal(InputGeometry.LocalToAbsolute(FVector2D(Input->CaretX(), 0)));
	PopupSize = FVector2D(FMath::Min(500.f, FMath::Max(100.f, Geometry.GetLocalSize().X - 64.f)),
		FMath::Min(220.f, 8.f + Completions.Num() * 30.f));
	PopupPosition = FVector2D(FMath::Clamp(Anchor.X, 0.f, FMath::Max(0.f, Geometry.GetLocalSize().X - PopupSize.X)),
		FMath::Max(0.f, Anchor.Y - PopupSize.Y - 6.f));
}
