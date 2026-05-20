#ifndef INIT_H
#define INIT_H

void setInitPhaseComplete(void);
void closeKeyboardIfOpened(void);

void Reset(void);
void loadUsbModules(void);
void setupPowerOff(void);
void closeAllAndPoweroff(void);
void startKbd(void);
void ensureCoreIoStackReady(void);

#ifdef ETH
void loadNetModules(void);
#endif

#endif
