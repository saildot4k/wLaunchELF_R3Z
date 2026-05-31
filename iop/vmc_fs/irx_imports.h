/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: irx_imports.h 377 2004-10-25 15:59:28Z tentacle $
# Defines all IRX imports.
*/

#ifndef IOP_IRX_IMPORTS_H
#define IOP_IRX_IMPORTS_H

#include "irx.h"

/*
 * vmc_fs registers with the explicit iomanX symbols.  PS2SDK defaults to
 * old AddDrv/DelDrv compatibility on IOP builds, so disable that before
 * importing iomanX.h or the generated import stubs won't match vmc_fs.c.
 */
#define IOMANX_OLD_NAME_ADDDELDRV 0

/* Please keep these in alphabetical order!  */
#include "cdvdman.h"
#include "intrman.h"
#include "iomanX.h"
#include "stdio.h"
#include "sysclib.h"
#include "sysmem.h"

#endif /* IOP_IRX_IMPORTS_H */
