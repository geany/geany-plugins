#ifndef GEANYPY_SCINTILLA_H__
#define GEANYPY_SCINTILLA_H__

PyTypeObject NotificationType;
PyTypeObject NotifyHeaderType;

typedef struct
{
	PyObject_HEAD
	ScintillaObject *sci;
} Scintilla;

typedef struct
{
	PyObject_HEAD
    SCNotification *notif;
} NotifyHeader;

typedef struct
{
    PyObject_HEAD
    SCNotification *notif;
    NotifyHeader *hdr;
} Notification;


PyMODINIT_FUNC init_geany_scintilla(void);
Scintilla *Scintilla_create_new_from_scintilla(ScintillaObject *sci);
Notification *Notification_create_new_from_scintilla_notification(SCNotification *notif);
NotifyHeader *NotifyHeader_create_new_from_scintilla_notification(SCNotification *notif);

#endif /* GEANYPY_SCINTILLA_H__ */
