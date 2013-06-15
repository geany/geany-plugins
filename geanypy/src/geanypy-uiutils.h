#ifndef GEANYPY_UI_UTILS_H__
#define GEANYPY_UI_UTILS_H__

PyTypeObject InterfacePrefsType;
PyTypeObject MainWidgetsType;

typedef struct
{
	PyObject_HEAD
	GeanyInterfacePrefs *iface_prefs;
} InterfacePrefs;

typedef struct
{
	PyObject_HEAD
	GeanyMainWidgets *main_widgets;
} MainWidgets;

#endif /* GEANYPY_UI_UTILS_H__ */
