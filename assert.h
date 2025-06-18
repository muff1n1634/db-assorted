#ifndef NW4R_DB_ASSERT_H
#define NW4R_DB_ASSERT_H

/*******************************************************************************
 * headers
 */

#include <cstdarg> // std::va_list

#include <macros.h> // ATTR_WEAK
#include <types.h> // u32

/*******************************************************************************
 * functions
 */

namespace nw4r { namespace db
{
	ATTR_WEAK void VPanic(char const *file, int line, char const *fmt,
	            std::va_list vlist);
	ATTR_WEAK void Panic(char const *file, int line, char const *fmt, ...);

	ATTR_WEAK void VWarning(char const *file, int line, char const *fmt,
	              std::va_list vlist);
	ATTR_WEAK void Warning(char const *file, int line, char const *fmt, ...);

	void Assertion_ShowConsole(u32 time);
}} // namespace nw4r::db

#endif // NW4R_DB_ASSERT_H
