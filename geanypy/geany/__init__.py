"""
Package file that exposes some of Geany's guts as its own attibutes.  Any
objects where it only makes sense to have one instance of are initialed here
and set as attributes.

You can sort of think of this file as the GeanyData struct from the C API,
though no real attempt is made to mimic that structure here.
"""

import app
import console
import dialogs
import document
import editor
import encoding
import filetypes
import highlighting
import loader
import main
import manager
import msgwindow
import navqueue
import prefs
import project
import scintilla
import search
import templates
import ui_utils

from app import App
from prefs import Prefs, ToolPrefs
from main import is_realized, locale_init, reload_configuration
from signalmanager import SignalManager
from ui_utils import MainWidgets, InterfacePrefs
from search import SearchPrefs
from templates import TemplatePrefs


__all__ = [ "Plugin",
            "is_realized",
            "locale_init",
            "reload_configuration",
            "main_widgets",
            "interface_prefs",
            "app",
            "general_prefs",
            "search_prefs",
            "template_prefs",
            "tool_prefs",
            "signals" ]

# Geany's application data fields
app = App()

# Import GTK+ widgets that are part of Geany's UI
main_widgets = MainWidgets()

# Preferences
general_prefs = Prefs() # GeanyData->prefs but name clashes with module
interface_prefs = InterfacePrefs()
search_prefs = SearchPrefs()
template_prefs = TemplatePrefs()
tool_prefs = ToolPrefs()

# GObject to connect signal handlers on and which emits signals.
signals = SignalManager()

import plugin
from plugin import Plugin
