#pragma once

#include "CoreMinimal.h"

class SHAREDTYPES_API FParsingException
{
	FString ErrorMessage;
	uint32 Position;

public:
	FParsingException(const FString& ErrorMessage, const uint32 InPosition) : ErrorMessage(ErrorMessage), Position(InPosition) {}
	virtual ~FParsingException() {}
	
	virtual FString GetErrorMessage() const;
	uint32 GetPosition() const;
};

class SHAREDTYPES_API FEOL : public FParsingException
{
public:
	FEOL() : FParsingException("Unexpected end of line", -1) {}
};

class SHAREDTYPES_API FUnexpectedSymbolException : public FParsingException
{
	TCHAR Symbol;
public:
	FUnexpectedSymbolException(const TCHAR& Symbol, const uint32 InPosition) :
		FParsingException("Unexpected symbol", InPosition),
		Symbol(Symbol)
	{
	}
	
	const TCHAR& GetSymbol() const;
	virtual FString GetErrorMessage() const override;
};

/**
 * a simple parser base
 *
 * note:
 * this parser has a special contract for reading expected characters:
 * - when you call the 'DoRead...' functions, they WILL return the required thing or throw
 * - if a function is called 'TryRead...', it will not throw
 */
class SHAREDTYPES_API FParserBase
{
	FString ParsedString;					// the string being parsed
	uint32 Cursor = 0;						// a position in text where next read will happen
	TOptional<FParsingException> Error;		// parsing error if any occurred

	// resets the parser state for the new read
	void Reset(const FString& NewString);
	
protected:
	/**
	 * whether the next read will result in EOL
	 * @return
	 */
	bool IsEOL() const
	{
		return !ParsedString.IsValidIndex(Cursor);
	}
	
	/**
	 * accumulates characters into accumulator and returns the accumulator transformed into some value
	 *
	 * if accumulator is still empty when EOL is reached or a test condition returns 'false', an exception will be thrown, provided by the exception factory
	 *
	 * use this function when a value is REQUIRED, i.e. the accumulator is NOT allowed to be empty
	 * @tparam U a function taking a char and returning a bool
	 * @tparam S a function taking a string a returning some value
	 * @tparam E a function that accepts a char and a position and returns an exception object
	 * @param AccumulateCondition test whether to accumulate a given char
	 * @param ResultTransform transform the accumulator into a value
	 * @param ExceptionFactory a factory that creates an exception object given a char and a position in text where the exception occurred
	 * @param SkipBlanks whether to ignore preceding blanks
	 * @param MaxLen accumulate only this many chars, then return
	 * @return 
	 */
	template<typename U, typename S, typename E>
	auto DoRead(U AccumulateCondition, S ResultTransform, E ExceptionFactory, bool SkipBlanks = true, int MaxLen = -1);
	
	/**
	 * reads characters enclosed between 'GroupOpenChar' and 'GroupCloseChar', INCLUDING the open
	 * and closing chars
	 *
	 * this can be used to read expressions enclosed in brackets
	 *
	 * nested groups are naturally accepted but they must be properly closed
	 * @param GroupOpenChar char used to open the group (e.g. '(')
	 * @param GroupCloseChar char used to close the group (e.g. ')')
	 * @param bSkipPrecedingBlanks 
	 * @return 
	 */
	FString DoReadEnclosedGroup(const TCHAR& GroupOpenChar, const TCHAR& GroupCloseChar, bool bSkipPrecedingBlanks = true);
	
	/**
	 * reads a valid integer or throws
	 * @param SkipPrecedingBlanks skip blanks preceding the integer
	 * @return 
	 */
	int DoReadInt(bool SkipPrecedingBlanks = true);

	/**
	 * reads a valid identifier (alnum + '_') or throws
	 * @param SkipPrecedingBlanks skip blanks preceding the identifier
	 * @return 
	 */
	FString DoReadIdentifier(bool SkipPrecedingBlanks = true);

	/**
	 * reads the required character or throws
	 * @param SkipPrecedingBlanks skip blanks preceding the char
	 * @param Char the required char
	 */
	TCHAR DoReadChar(const TCHAR& Char, bool SkipPrecedingBlanks = true);

	/**
	 * returns the next character that will be read without moving the cursor, \0 if EOL
	 *
	 * note: this method never throws
	 * @param SkipPrecedingBlanks skip preceding blanks
	 * @return
	 */
	TCHAR Peak(bool SkipPrecedingBlanks = true) const;

	/**
	 * peaks the character at an offset
	 *
	 * return '\0' for invalid indices
	 * @param Offset positive or negative offset for the peak. Offset = 0 peaks the next char that will be read and so on
	 * @return 
	 */
	TCHAR Peak(const int32 Offset) const
	{
		return ParsedString.IsValidIndex(Cursor + Offset) ? ParsedString[Cursor + Offset] : '\0';
	}
	
	/**
	 * reads next character or throws EOL
	 * @return 
	 */
	TCHAR DoReadNext();

	/**
	 * reads next character unsafely. can result in out-of-bounds if not used properly
	 * @return 
	 */
	FORCEINLINE TCHAR ReadNextUnchecked()
	{
		return ParsedString[Cursor++];
	}

	/**
	 * reads at least one non-blank character or throws
	 * @param SkipPrecedingBlanks 
	 * @return 
	 */
	FString DoReadNonBlank(bool SkipPrecedingBlanks = true);
	
	/**
	 * reads a piece of text enclosed in quote marks, throws if
	 * - the first read char is unexpected (not a quotation mark)
	 * - EOL is reached before the closing quotation mark is encountered
	 * @param SkipPrecedingBlanks skip the preceding blanks
	 * @param QuoteChar character used as quote mark
	 * @param EscapeChar the chararcter used to escape quote chars
	 * @return 
	 */
	FString DoReadQuotedString(bool SkipPrecedingBlanks = true, const TCHAR& QuoteChar = '"', const TCHAR& EscapeChar = '\\');

	/**
	 * actual parsing implementation, you are allowed to throw in this method
	 */
	virtual void ParseImpl() = 0;

public:
	bool Parse(const FString& Text);
	bool HasError() const;
	const FParsingException* GetError() const;
	virtual ~FParserBase() = default;
};
