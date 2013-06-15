"""
A simple analog of `GeanyObject` in the C API, that is, an object to emit
all signals on.  The signals are emitted from the C code in signalmanager.c,
where the Geany types get wrapped in PyObject types.
"""

import gobject


class SignalManager(gobject.GObject):
	"""
	Manages callback functions for events emitted by Geany's internal GObject.
	"""
	__gsignals__ = {
		'build-start':				(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										()),
		'document-activate':		(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'document-before-save':		(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'document-close':			(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'document-filetype-set':	(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT, gobject.TYPE_PYOBJECT)),
		'document-new':				(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'document-open':			(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'document-reload':			(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'document-save':			(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'editor-notify':			(gobject.SIGNAL_RUN_LAST, gobject.TYPE_BOOLEAN,
										(gobject.TYPE_PYOBJECT, gobject.TYPE_PYOBJECT)),
		'geany-startup-complete':	(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										()),
		'project-close': 			(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										()),
		'project-dialog-confirmed':	(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'project-dialog-open':		(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'project-dialog-close':		(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'project-open':				(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'project-save':				(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_PYOBJECT,)),
		'update-editor-menu':		(gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
										(gobject.TYPE_STRING, gobject.TYPE_INT,
										gobject.TYPE_PYOBJECT)),
	} # __gsignals__

	def __init__(self):
		self.__gobject_init__()

gobject.type_register(SignalManager)

