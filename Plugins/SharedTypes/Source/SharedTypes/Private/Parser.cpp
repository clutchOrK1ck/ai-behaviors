#include "Parser.h"

FString FParsingException::GetErrorMessage() const
{
	return ErrorMessage;
}

uint32 FParsingException::GetPosition() const
{
	return Position;
}

const TCHAR& FUnexpectedSymbolException::GetSymbol() const
{
	return Symbol;
}

FString FUnexpectedSymbolException::GetErrorMessage() const
{
	return FParsingException::GetErrorMessage() + " '" + Symbol + "'";
}

void FParserBase::Reset(const FString& NewString)
{
	Cursor = 0;
	Error.Reset();
	ParsedString = NewString;
}

bool FParserBase::Parse(const FString& Text)
{
	Reset(Text);

	try
	{
		ParseImpl();
	} catch (const FParsingException& Exception)
	{
		Error.Emplace(Exception);
		return false;
	}

	return true;
}

bool FParserBase::HasError() const
{
	return Error.IsSet();
}

const FParsingException* FParserBase::GetError() const
{
	return Error.GetPtrOrNull();
}

template <typename U, typename S, typename E>
auto FParserBase::DoRead(U AccumulateCondition, S ResultTransform, E ExceptionFactory, bool SkipBlanks, int MaxLen)
{
	FString Accumulator;
	int Cursor_;

	for (Cursor_ = Cursor; Cursor_ < ParsedString.Len(); Cursor_++)
	{
		const auto Char = ParsedString[Cursor_];

		const bool bUnexpectedChar = !AccumulateCondition(Char);
		
		if (bUnexpectedChar)
		{
			// encountering an unexpected char after reading some means we have finished reading the required thing
			if (!Accumulator.IsEmpty())
			{
				break;
			}

			// this will only catch preceding blanks, because if the accum is not empty, we have broken above already
			if (FChar::IsWhitespace(Char) && SkipBlanks)
			{
				continue;
			}

			auto Exception = ExceptionFactory(Char, Cursor_);
			throw Exception;
		}
		
		Accumulator += Char;

		if (MaxLen > 0 && Accumulator.Len() >= MaxLen)
		{
			Cursor_++;
			break;
		} 
	}

	if (Accumulator.IsEmpty())
	{
		throw FEOL();
	}
	
	Cursor = Cursor_;
	return ResultTransform(Accumulator);
}

FString FParserBase::DoReadEnclosedGroup(const TCHAR& GroupOpenChar, const TCHAR& GroupCloseChar, bool bSkipPrecedingBlanks)
{
	FString Group;

	Group += DoReadChar(GroupOpenChar, bSkipPrecedingBlanks);

	// start reading the group contents char by char
	uint32 NestingDepth = 0;
	auto Next = Peak(false);
	
	while (Next != '\0')
	{
		Group += ReadNextUnchecked();

		// nesting
		if (Next == GroupCloseChar)
		{
			if (NestingDepth == 0)
			{
				return Group;
			}

			NestingDepth--;
		} else if (Next == GroupOpenChar)
		{
			NestingDepth++;
		}

		Next = Peak(false);
	}

	throw FEOL();
}

int FParserBase::DoReadInt(bool bSkipBlanks)
{
	return DoRead(
		FChar::IsDigit,
		[](const FString& Accumulator)
		{
			return FCString::Atoi(*Accumulator);
		},
		[](const TCHAR& Char, const int Cursor)
		{
			return FParsingException("Expected integer", Cursor);
		},
		bSkipBlanks, -1);
}

FString FParserBase::DoReadIdentifier(bool bSkipBlanks)
{
	return DoRead(
		FChar::IsIdentifier,
		[](const FString& Accumulator)
		{
			return Accumulator;
		},
		[](const TCHAR& Char, const int Cursor)
		{
			return FParsingException("Expected identifier", Cursor);
		},
		bSkipBlanks, -1);
}

TCHAR FParserBase::DoReadChar(const TCHAR& Char, bool bSkipBlanks)
{
	return DoRead(
		[&Char](const TCHAR& TestChar)
		{
			return TestChar == Char;
		},
		[](const FString& Accumulator)
		{
			return Accumulator[0];
		},
		[&Char](const TCHAR& Char_, const int Cursor)
		{
			FString ErrorMessage = FString::Printf(TEXT("Expected '%c', received '%c'"), Char, Char_);
			return FParsingException(ErrorMessage, Cursor);
		},
		bSkipBlanks, 1
		);
}

TCHAR FParserBase::Peak(bool SkipPrecedingBlanks) const
{
	for (int Cursor_ = Cursor; Cursor_ < ParsedString.Len(); Cursor_++)
	{
		const auto Char = ParsedString[Cursor_];
		
		if (FChar::IsWhitespace(Char) && SkipPrecedingBlanks)
		{
			continue;
		}

		return Char;
	}

	return '\0';
}

TCHAR FParserBase::DoReadNext()
{
	if (IsEOL())
	{
		throw FEOL();
	}

	TCHAR Read = ParsedString[Cursor];
	Cursor++;
	return Read;
}

FString FParserBase::DoReadNonBlank(bool SkipPrecedingBlanks)
{
	return DoRead(
		[](const TCHAR& Char)
		{
			return !FChar::IsWhitespace(Char);
		},
		[](const FString& Accumulator)
		{
			return Accumulator;
		},
		[](const TCHAR& Char, const int Cursor)
		{
			return FParsingException("", -1); // impossible
		},
		SkipPrecedingBlanks, -1);
}

FString FParserBase::DoReadQuotedString(bool SkipPrecedingBlanks,
                                      const TCHAR& QuoteChar,
                                      const TCHAR& EscapeChar)
{
	// consume the opening quote
	DoReadChar(QuoteChar, SkipPrecedingBlanks);

	FString QuotedString;
	TCHAR Next = Peak(false);

	while (Next != '\0')
	{
		// escaped quote (do not read the escape char)
		if (Next == EscapeChar && Peak(1) == QuoteChar)
		{
			ReadNextUnchecked(); // skip the escape
			QuotedString += ReadNextUnchecked();
		} else if (Next == QuoteChar)
		{
			// move cursor past the closing quote
			ReadNextUnchecked();
			return QuotedString;
		} else
		{
			QuotedString += ReadNextUnchecked();
		}
		
		Next = Peak(false);
	}

	throw FEOL();
}
