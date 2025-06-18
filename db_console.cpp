#include <nw4r/db/console.h>

/*******************************************************************************
 * headers
 */

#include <cstdarg>
#include <cstdio> // vsnprintf

#include <macros.h>
#include <types.h>

#include <nw4r/db/directPrint.h>

#include <revolution/OS/OSError.h> // OSReport
#include <revolution/OS/OSMutex.h>
#include <revolution/OS/OSInterrupt.h>
#include <revolution/OS/OSThread.h> // OSGetCurrentThread

#include <nw4r/NW4RAssert.h>

/*******************************************************************************
 * local function declarations
 */

namespace nw4r { namespace db
{
	static inline u8 *GetTextPtr_(detail::ConsoleHead *console, u16 line,
	                                  u16 xPos)
	{
		return console->textBuf + xPos + (console->width + 1) * line;
	}

	static inline u32 CodeWidth_(u8 const *p)
	{
		return *p >= 0x81 ? sizeof(wchar_t) : sizeof(char);
	}

	static inline u32 GetTabSize_(detail::ConsoleHead *console)
	{
		s32 tab = (console->attr & REGISTER16_BITFIELD(12, 13)) >> 2;
		// EXTRACT_BIT_FIELD does not generate srawi

		return static_cast<u32>(2 << tab);
	}

	static inline u8 const *SearchEndOfLine_(u8 const *str)
	{
		while (*str != '\n' && *str != '\0')
			str++;

		return str;
	}

	static inline u16 GetRingUsedLines_(detail::ConsoleHead *console)
	{
		NW4RAssertPointerNonnull_Line(108, console);

		{ // 39ac92 wants lexical_block
			s32 lines = console->printTop - console->ringTop;

			if (lines < 0)
				lines += console->height;

			return static_cast<u16>(lines);
		}
	}

	static inline u16 GetActiveLines_(detail::ConsoleHead *console)
	{
		u16 lines = GetRingUsedLines_(console);

		if (console->printXPos)
			lines++;

		return lines;
	}

	static void TerminateLine_(detail::ConsoleHead *console);
	static u8 *NextLine_(detail::ConsoleHead *console);
	static u8 *PutTab_(detail::ConsoleHead *console, u8 *dstPtr);
	static u32 GetTabSize_(detail::ConsoleHead *console);
	static u32 PutChar_(detail::ConsoleHead *console, const u8 *str, u8 *dstPtr);
	static u32 CodeWidth_(const u8 *p);

	static void UnlockMutex_(OSMutex *mutex);
	static bool TryLockMutex_(OSMutex *mutex);

	static void DoDrawString_(detail::ConsoleHead *console, u32 printLine,
	                          u8 const *str, ut::TextWriterBase<char> *writer);
	static void DoDrawConsole_(detail::ConsoleHead *console,
	                           ut::TextWriterBase<char> *writer);

	static void PrintToBuffer_(detail::ConsoleHead *console, u8 const *str);

	static void Console_PrintString_(ConsoleOutputType type,
	                                 detail::ConsoleHead *console,
	                                 u8 const *str);
}} // namespace nw4r::db

/*******************************************************************************
 * variables
 */

namespace nw4r { namespace db
{
	static OSMutex sMutex;
}} // namespace nw4r::db

/*******************************************************************************
 * functions
 */

namespace nw4r { namespace db {

static void TerminateLine_(detail::ConsoleHead *console)
{
	*GetTextPtr_(console, console->printTop, console->printXPos) = '\0';
}

static u8 *NextLine_(detail::ConsoleHead *console)
{
	*GetTextPtr_(console, console->printTop, console->printXPos) = '\0';
	console->printXPos = 0;
	console->printTop++;

	if (console->printTop == console->height
        && !(console->attr & FLAG_BIT(1)))
		console->printTop = 0;

	if (console->printTop == console->ringTop)
	{
		console->ringTopLineCnt++;

		if (++console->ringTop == console->height)
			console->ringTop = 0;
	}

	return GetTextPtr_(console, console->printTop, 0);
}

static u8 *PutTab_(detail::ConsoleHead *console, u8 *dstPtr)
{
	u32 tabWidth = GetTabSize_(console);

	do
	{
		*dstPtr++ = ' ';
		console->printXPos++;

		if (console->printXPos >= console->width)
			break;
	} while (console->printXPos & (tabWidth - 1));

	return dstPtr;
}

static u32 PutChar_(detail::ConsoleHead *console, u8 const *str, u8 *dstPtr)
{
	u32 codeWidth = CodeWidth_(str);
	u32 cnt;

	ensure(console->printXPos + codeWidth <= console->width, 0);

	console->printXPos += static_cast<u16>(codeWidth);

	for (cnt = codeWidth; cnt; cnt--)
		*dstPtr++ = *str++;

	return codeWidth;
}

// dwarf line is 300?
static void UnlockMutex_(OSMutex *mutex)
{
	if (OSGetCurrentThread())
		OSUnlockMutex(mutex);
}

// dwarf line is 273?
static bool TryLockMutex_(OSMutex *mutex)
{
	if (OSGetCurrentThread())
	{
		OSLockMutex(mutex);
		return true;
	}
	else if (!mutex->thread)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void DoDrawString_(detail::ConsoleHead *console, u32 printLine,
                          u8 const *str, ut::TextWriterBase<char> *writer)
{
	if (writer)
	{
		writer->Printf("%s\n", str);
	}
	else
	{
		s32 height = (s32)((u32)console->viewPosY + printLine * 10);

		DirectPrint_DrawString(console->viewPosX, height, false, "%s\n", str);
	}
}

static void DoDrawConsole_(detail::ConsoleHead *console,
                           ut::TextWriterBase<char> *writer)
{
	// was this meant to be an if statement?
	TryLockMutex_(&sMutex);
	{ // 39ab35 wants lexical_block
		s32 viewOffset;
		u16 line;
		u16 printLines;
		u16 topLine;

		viewOffset = console->viewTopLine - console->ringTopLineCnt;
		printLines = 0;

		if (viewOffset < 0)
			viewOffset = 0;
		else if (viewOffset > GetActiveLines_(console))
			goto end;

		line = static_cast<u16>(console->ringTop + viewOffset);

		if (line >= console->height)
			line -= console->height;

		topLine =
			console->printTop + BOOLIFY_TERNARY_TYPE(u16, console->printXPos);

		if (topLine == console->height)
			topLine = 0;

		while (line != topLine)
		{
			DoDrawString_(console, printLines, GetTextPtr_(console, line, 0),
			              writer);

			printLines++;
			line++;

			if (line == console->height)
			{
				if (console->attr & FLAG_BIT(1))
					goto end;

				line = 0;
			}

			if (printLines >= console->viewLines)
				goto end;
		}
	}

	// maybe not, with this end label?
end:
	UnlockMutex_(&sMutex);
}

void Console_DrawDirect(detail::ConsoleHead *console)
{
	NW4RAssertPointerNonnull_Line(682, console);

	if (DirectPrint_IsActive() && console->isVisible)
	{
		int width = console->width * 6 + 12,
			height = console->viewLines * 10 + 4;

		DirectPrint_EraseXfb(console->viewPosX - 6, console->viewPosY - 3,
		                     width, height);
		DoDrawConsole_(console, nullptr);
		DirectPrint_StoreCache();
	}
}

static void PrintToBuffer_(detail::ConsoleHead *console, u8 const *str)
{
	u8 *storePtr ATTR_UNUSED;

	NW4RAssertPointerNonnull_Line(806, console);
	NW4RAssertPointerNonnull_Line(807, str);

	bool_t intrStatus = OSDisableInterrupts(); /* int enabled; */

	storePtr = GetTextPtr_(console, console->printTop, console->printXPos);

	while (*str)
	{
		if (console->attr & 1 && console->printTop == console->height)
			break;

		while (*str) // ? just use continue? am i missing something?
		{
			bool newLineFlag = false;

			if (*str == '\n')
			{
				str++;
				storePtr = NextLine_(console);
				break;
			}

			if (*str == '\t')
			{
				str++;
				storePtr = PutTab_(console, storePtr);
			}
			else
			{
				u32 bytes = PutChar_(console, str, storePtr);

				if (bytes)
				{
					str += bytes;
					storePtr += bytes;
				}
				else
					{ newLineFlag = true; }
			}

			if (console->printXPos >= console->width)
				newLineFlag = true;

			if (newLineFlag)
			{
				if (console->attr & 1)
				{
					str = SearchEndOfLine_(str);
					break;
				}

				if (*str == '\n')
					str++;

				storePtr = NextLine_(console);
				break;
			}

			if (!*str)
				TerminateLine_(console);
		}
	}

	OSRestoreInterrupts(intrStatus);
}

static void Console_PrintString_(ConsoleOutputType type,
                                 detail::ConsoleHead *console, u8 const *str)
{
	NW4RAssertPointerNonnull_Line(909, console);

	if (type & CONSOLE_OUTPUT_DISPLAY)
		OSReport("%s", str);

	if (type & CONSOLE_OUTPUT_TERMINAL)
		PrintToBuffer_(console, str);
}

void Console_VFPrintf(ConsoleOutputType type ATTR_UNUSED,
                      detail::ConsoleHead *console ATTR_UNUSED,
                      char const *format ATTR_UNUSED,
                      std::va_list vlist ATTR_UNUSED)
{
#if !defined(NDEBUG)
	static int dummy ATTR_UNUSED; // needed to get the @0 at the end of sStrBuf
	static u8 sStrBuf[1024];

	NW4RAssertPointerNonnull_Line(941, console);

	if (TryLockMutex_(&sMutex))
	{
		// Cool
		std::vsnprintf(reinterpret_cast<char *>(sStrBuf), sizeof sStrBuf,
		               format, vlist);

		Console_PrintString_(type, console, sStrBuf);

		UnlockMutex_(&sMutex);
	}
#endif // !defined(NDEBUG)
}

void Console_Printf(detail::ConsoleHead *console ATTR_UNUSED,
                    char const *format ATTR_UNUSED, ...)
{
	std::va_list vlist;

	va_start(vlist, format);
#if NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
	Console_VFPrintf(CONSOLE_OUTPUT_ALL, console, format, vlist);
#endif // NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
	va_end(vlist);
}

s32 Console_GetTotalLines(detail::ConsoleHead *console)
{
	s32 count;

	NW4RAssertPointerNonnull_Line(1128, console);

	bool_t intrStatus = OSDisableInterrupts(); /* int enabled; */

	count = GetActiveLines_(console) + console->ringTopLineCnt;

	OSRestoreInterrupts(intrStatus);

	return count;
}

}} // namespace nw4r::db
