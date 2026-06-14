#ifndef INIT_H
#define INIT_H

void setInitPhaseComplete(void);
void closeKeyboardIfOpened(void);

void Reset(void);
void loadUsbModules(void);
int ensureUsbKeyboardReady(void);
#ifdef DS34
void loadDs34InputModules(void);
#endif
void setupPowerOff(void);
void poweroffConsole(void);
void closeAllAndPoweroff(void);
void startKbd(void);
void ensureCoreIoStackReady(void);
void rebootIopAndReloadCoreStack(void);

#ifdef ETH
void loadNetModules(void);
#endif
#ifdef UDPFS
int reloadUdpfsModules(void);
#endif

#endif
