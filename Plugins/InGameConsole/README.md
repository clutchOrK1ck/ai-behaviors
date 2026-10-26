# Native console UI

The console is a native Slate overlay. No Widget Blueprint or widget-class setup is required.
The configured `GameConsoleSettings.ToggleKey` (Tilde by default) opens and closes it in the
focused game viewport. A viewport-scoped Slate input processor handles the key before Unreal's
built-in console. `UGameConsole::ShowUI()` / `HideUI()` are also available from C++.

The console still uses the existing command sets, parser, execution history, and executors.
Its UI does not register commands automatically; register your command sets as before.

| Control | Action |
| --- | --- |
| Enter | Submit the input; with suggestions open, accept the selected completion first |
| Tab | Request suggestions; press again to cycle forward |
| Shift+Tab | Cycle suggestions backward |
| Up / Down | Navigate suggestions, or history when the popup is closed |
| Escape | Dismiss suggestions; otherwise close the console |
| Page Up / Page Down | Scroll the output |
| Mouse wheel | Scroll the output under the pointer |

The input uses Slate's native single-line editor: caret navigation, selection, Backspace,
Delete, clipboard shortcuts, and normal editing remain available. History navigation restores
the unfinished draft when returning to the newest entry. Editing a recalled entry starts a new
draft.

Output is buffered in arrival order, including while the overlay is hidden. Normal output is
light gray, stderr is red, and submitted input is green. New output follows the bottom unless
the user has scrolled away. Submitting input returns the view to the bottom. Console reset
clears both the buffer and the rendered view. Closing preserves the input draft and scroll
position. The previous keyboard focus, cursor visibility, and paired look/move-input ignore
state are restored on close.

Autocomplete inserts the suffix returned by `SuggestAutoCompletions` at the caret, preserving
the text after it. Results are sorted for predictable navigation. Existing parser/autocomplete
limitations (including the deferred option-name-as-value ambiguity) are unchanged.

The `InGameConsole.Slate.TerminalInteraction` automation test covers cross-stream output
buffering/replay, live updates, reset, history boundaries, completion insertion, and suggestion
cycling. Real viewport focus, popup placement, scrolling, and rendering should also be checked
in PIE. `ConsoleWidget.h/.cpp` and the old `SetWidgetClass` Blueprint API have been removed;
remove any old Blueprint calls to that API.
