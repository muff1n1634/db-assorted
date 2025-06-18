#ifndef NW4R_CONFIG_H
#define NW4R_CONFIG_H

/*******************************************************************************
 * options
 */

/* NW4R library version used by DVD games. Contains support for displaying
 * symbols from map files stored on disc.
 */
#define NW4R_APP_TYPE_DVD	2

/* NW4R library version used by NAND games (WiiWare, VC). Does not contain
 * such support.
 */
#define NW4R_APP_TYPE_NAND	3

/*******************************************************************************
 * configuration
 */

/* The macro NW4R_APP_TYPE must be defined to one of these two values.
 *
 * You can use one of the defines here or set NW4R_APP_TYPE in your build system.
 */
// #define NW4R_APP_TYPE	NW4R_APP_TYPE_DVD
// #define NW4R_APP_TYPE	NW4R_APP_TYPE_NAND

#ifndef NW4R_APP_TYPE
# error NW4R_APP_TYPE was not configured. See NW4RConfig.h for details.
#endif

#endif // NW4R_CONFIG_H
