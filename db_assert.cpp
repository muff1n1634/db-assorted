#include <nw4r/db/assert.h>

/*******************************************************************************
 * headers
 */

#include <nw4r/NW4RConfig.h>

#include <cstdarg>

#include <macros.h>
#include <types.h>

#include <nw4r/db/console.h>
#include <nw4r/db/directPrint.h>
#include <nw4r/db/mapFile.h>

#include <revolution/OS/OSAlarm.h>
#include <revolution/OS/OSContext.h>
#include <revolution/OS/OSError.h>
#include <revolution/OS/OSInterrupt.h>
#include <revolution/OS/OSMemory.h>
#include <revolution/OS/OSThread.h>
#include <revolution/BASE/PPCArch.h>
#include <revolution/VI/vi.h>

/*******************************************************************************
 * local function declarations
 */

namespace nw4r { namespace db
{
	static void Assertion_Printf_(char const *fmt, ...);

#if NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
	static bool ShowMapInfoSubroutine_(u32 address, bool preCRFlag);
#endif // NW4R_APP_TYPE == NW4R_APP_TYPE_DVD

	ATTR_NOINLINE static void ShowStack_(register_t sp);

	static OSAlarm &GetWarningAlarm_();
	static void WarningAlarmFunc_(OSAlarm *, OSContext *);
}} // namespace nw4r::db

/*******************************************************************************
 * variables
 */

namespace nw4r { namespace db
{
	static u32 sWarningTime;
	static detail::ConsoleHead *sAssertionConsole;
	static bool sDispWarningAuto = true;
}} // namespace nw4r::db

/*******************************************************************************
 * functions
 */

namespace nw4r { namespace db {

static void Assertion_Printf_(char const *fmt, ...)
{
	std::va_list vlist;

	va_start(vlist, fmt);

#if NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
	if (sAssertionConsole)
		Console_VPrintf(sAssertionConsole, fmt, vlist);
	else
		OSVReport(fmt, vlist);
#elif NW4R_APP_TYPE == NW4R_APP_TYPE_NAND
	if (!sAssertionConsole)
		OSVReport(fmt, vlist);
#endif // NW4R_APP_TYPE

	va_end(vlist);
}

#if NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
static bool ShowMapInfoSubroutine_(u32 address, bool preCRFlag)
{
	ensure(MapFile_Exists(), false);
	ensure(0x80000000 <= address && address <= 0x82ffffff, false);

	{ // 39f796 wants lexical_block
		u8 strBuf[260];
		bool result = MapFile_QuerySymbol(address, strBuf, sizeof strBuf);

		if (result)
		{
			if (preCRFlag)
				Assertion_Printf_("\n");

			Assertion_Printf_("%s\n", strBuf);

			return true;
		}
		else
			{ return false; }
	}
}
#endif // NW4R_APP_TYPE == NW4R_APP_TYPE_DVD

ATTR_NOINLINE static void ShowStack_(register_t sp)
{
	u32 i;
	register_t *p;

	Assertion_Printf_("-------------------------------- TRACE\n");
	Assertion_Printf_("Address:   BackChain   LR save\n");

	p = reinterpret_cast<register_t *>(sp);

	for (i = 0; i < 16; i++)
	{
		if (reinterpret_cast<u32>(p) == 0x00000000)
			break;

		if (reinterpret_cast<u32>(p) == 0xffffffff)
			break;

		if (!(reinterpret_cast<u32>(p) & 0x80000000))
			break;

		// clang-format off
		Assertion_Printf_("%08X:  %08X    %08X ",
		                   p,     p[0],   p[1]);
		// clang-format on

#if NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
		if (!ShowMapInfoSubroutine_(p[1], false))
#endif // NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
			Assertion_Printf_("\n");

		p = reinterpret_cast<register_t *>(*p);
	}
}

ATTR_WEAK ATTR_POSS_NORETURN void VPanic(char const *file, int line,
                                         char const *fmt, std::va_list vlist,
                                         bool halt)
{
	register register_t stackPointer;

	asm { mr stackPointer, r1 } // not OSGetStackPointer?

	stackPointer = *reinterpret_cast<register_t *>(stackPointer);

	OSDisableInterrupts();
	OSDisableScheduler();

	VISetPreRetraceCallback(nullptr);
	VISetPostRetraceCallback(nullptr);

#if NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
	if (sAssertionConsole)
		detail::DirectPrint_SetupFB(nullptr);
#endif // NW4R_APP_TYPE == NW4R_APP_TYPE_DVD

	ShowStack_(stackPointer);

	if (sAssertionConsole)
	{
		Console_Printf(sAssertionConsole, "%s:%d Panic:", file, line);
#if !defined(NDEBUG)
		Console_VPrintf(sAssertionConsole, fmt, vlist);
#endif // !defined(NDEBUG)
		Console_Printf(sAssertionConsole, "\n");

		Console_ShowLatestLine(sAssertionConsole);
		Console_SetVisible(sAssertionConsole, true);
#if NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
		Console_DrawDirect(sAssertionConsole);
#endif // NW4R_APP_TYPE == NW4R_APP_TYPE_DVD
	}
	else
	{
		OSReport("%s:%d Panic:", file, line);
		OSVReport(fmt, vlist);
		OSReport("\n");
	}

	if (halt)
		PPCHalt();
}

ATTR_WEAK void Panic(char const *file, int line, char const *msg, ...)
{
	std::va_list vlist;

	va_start(vlist, msg);
	VPanic(file, line, msg, vlist, true);
	va_end(vlist);

	PPCHalt();
}

ATTR_WEAK void VWarning(char const *file, int line, char const *fmt,
                        std::va_list vlist)
{
	if (sAssertionConsole)
	{
		Console_Printf(sAssertionConsole, "%s:%d Warning:", file, line);
#if !defined(NDEBUG)
		Console_VPrintf(sAssertionConsole, fmt, vlist);
#endif // !defined(NDEBUG)
		Console_Printf(sAssertionConsole, "\n");

		Console_ShowLatestLine(sAssertionConsole);

		if (sDispWarningAuto)
			Assertion_ShowConsole(sWarningTime);
	}
	else
	{
		OSReport("%s:%d Warning:", file, line);
		OSVReport(fmt, vlist);
		OSReport("\n");
	}
}

ATTR_WEAK void Warning(char const *file, int line, char const *msg, ...)
{
	std::va_list vlist;

	va_start(vlist, msg);
	VWarning(file, line, msg, vlist);
	va_end(vlist);
}

void Assertion_ShowConsole(u32 time)
{
	ensure(sAssertionConsole);

	OSAlarm &alarm = GetWarningAlarm_();
	OSCancelAlarm(&alarm);

	Console_SetVisible(sAssertionConsole, true);

	if (time)
		OSSetAlarm(&alarm, time, &WarningAlarmFunc_);
}

static OSAlarm &GetWarningAlarm_()
{
	static bool sInitializedAlarm = false;
	static OSAlarm sWarningAlarm;

	if (!sInitializedAlarm)
	{
		OSCreateAlarm(&sWarningAlarm);
		sInitializedAlarm = true;
	}

	return sWarningAlarm;
}

static void WarningAlarmFunc_(OSAlarm *, OSContext *)
{
	if (sAssertionConsole)
		Console_SetVisible(sAssertionConsole, false);
}

}} // namespace nw4r::db
