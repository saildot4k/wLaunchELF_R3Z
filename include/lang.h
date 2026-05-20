/*
 * Intentionally no include guard:
 * this file is included multiple times with different `lang(...)` macro
 * definitions to generate both default string storage and lookup tables.
 */
#ifdef WLE_LANG_SPA
#include "Lang/SPA.LNG"
#endif

//#ifdef WLE_LANG_ENG
//#define CUSTOM_LNG
//#include "Lang/ENG.LNG"
//#endif

#ifndef CUSTOM_LNG
#include "Lang/ENG.LNG"
#endif
