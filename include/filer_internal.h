#ifndef FILER_INTERNAL_H
#define FILER_INTERNAL_H

#include "filer_shared.h"

extern char parties[MAX_PARTITIONS][MAX_PART_NAME + 1];

#ifdef DVRP
extern int ndvrpparties;
extern char mountedDVRPParty[MOUNT_LIMIT][MAX_NAME];
extern int latestDVRPMount;
#endif

#if defined(ETH) || defined(UDPFS)
extern int host_error;
extern int host_ready;
extern int host_use_Bsl;
#endif

void unmountHddPartiesNotNeededByClipboard(void);

#endif
