#ifndef SIGNALMANAGER_H
#define SIGNALMANAGER_H

typedef struct _SignalManager SignalManager;

SignalManager *signal_manager_new(GeanyPlugin *geany_plugin);
void signal_manager_free(SignalManager *signal_manager);
GObject *signal_manager_get_gobject(SignalManager *signal_manager);

#endif /* SIGNALMANAGER_H */
