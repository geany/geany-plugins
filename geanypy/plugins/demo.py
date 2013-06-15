import geany


class DemoPlugin(geany.Plugin):

    __plugin_name__ = "Demo Plugin"
    __plugin_version__ = "0.01"
    __plugin_description__ = "A Sample plugin"
    __plugin_author__ = "Matthew Brush <mbrush@codebrainz.ca>"

    def __init__(self):
        geany.Plugin.__init__(self)
        print("Demo plugin initializing")
        doc = geany.document.new_file()
        doc.editor.scintilla.set_text("Hello from the Demo plugin")

    def cleanup(self):
        print("Demo plugin cleaning up")
