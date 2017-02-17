# -*- coding: utf-8 -*-

from geany import glog
from traceback import format_exception
import logging
import sys


GLIB_LOG_LEVEL_MAP = {
	logging.DEBUG: glog.LOG_LEVEL_DEBUG,
	logging.INFO: glog.LOG_LEVEL_INFO,
	logging.WARNING: glog.LOG_LEVEL_WARNING,
	# error and critical levels are swapped on purpose because
	# GLib interprets CRITICAL less fatal than Python and additionally
	# GLib abort()s the program on G_LOG_LEVEL_ERROR which is uncommon
	# in Python
	logging.ERROR: glog.LOG_LEVEL_CRITICAL,
	logging.CRITICAL: glog.LOG_LEVEL_ERROR,
}


class PluginLogger(object):
	"""
	Convenience wrapper softly emulating Python's logging interface.
	Any log messages are passed to the GLib log system using g_log()
	and the LOG_DOMAIN is set automatically to the plugin's name for convenience.
	"""

	def __init__(self, plugin_name):
		self._log_domain = u'geanypy-%s' % plugin_name

	def debug(self, msg_format, *args, **kwargs):
		self.log(logging.DEBUG, msg_format, *args, **kwargs)

	def info(self, msg_format, *args, **kwargs):
		self.log(logging.INFO, msg_format, *args, **kwargs)

	def message(self, msg_format, *args, **kwargs):
		self.log(glog.LOG_LEVEL_MESSAGE, msg_format, *args, **kwargs)

	def warning(self, msg_format, *args, **kwargs):
		self.log(logging.WARNING, msg_format, *args, **kwargs)

	def error(self, msg_format, *args, **kwargs):
		self.log(logging.ERROR, msg_format, *args, **kwargs)

	def exception(self, msg_format, *args, **kwargs):
		kwargs['exc_info'] = True
		self.error(msg_format, *args, **kwargs)

	def critical(self, msg_format, *args, **kwargs):
		self.log(logging.CRITICAL, msg_format, *args, **kwargs)

	warn = warning
	fatal = critical

	def log(self, level, msg_format, *args, **kwargs):
		# map log level from Python to GLib
		glib_log_level = GLIB_LOG_LEVEL_MAP.get(level, glog.LOG_LEVEL_MESSAGE)
		# format message
		log_message = msg_format % args
		# log exception information if requested
		exc_info = kwargs.get('exc_info', False)
		if exc_info:
			traceback_text = self._format_exception(exc_info)
			log_message = '%s\n%s' % (log_message, traceback_text)

		glog.glog(self._log_domain, glib_log_level, log_message)

	def _format_exception(self, exc_info):
		if not isinstance(exc_info, tuple):
			exc_info = sys.exc_info()
		exc_text_lines = format_exception(*exc_info)
		return ''.join(exc_text_lines)
