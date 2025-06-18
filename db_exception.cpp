#include <nw4r/db/exception.h>

/*******************************************************************************
 * headers
 */

#include <types.h>

#include <nw4r/db/console.h>

/*******************************************************************************
 * types
 */

namespace nw4r { namespace db
{
	struct ExceptionCallbackParam;
}} // namespace nw4r::db

/*******************************************************************************
 * local function declarations
 */

namespace nw4r { namespace db
{
	/* Name: [RYWE01]/map/RevoYawarakaD.map:86234,86323
	 * For weak function ordering on debug
	 */
	void DumpException_(ExceptionCallbackParam const *);

	/* Name: [RYWE01]/map/RevoYawarakaD.map:86324,86326
	 * For weak function ordering on debug
	 */
	void DrawConsoleEndless_(detail::ConsoleHead *);
}} // namespace nw4r::db

/*******************************************************************************
 * functions
 */

namespace nw4r { namespace db {

void DumpException_(ExceptionCallbackParam const *)
{
	Console_SetVisible(nullptr, false);
}

void DrawConsoleEndless_(detail::ConsoleHead *)
{
	Console_SetViewBaseLine(nullptr, 0);
}

}} // namespace nw4r::db
