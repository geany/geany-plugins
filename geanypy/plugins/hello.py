import gtk
import geany

class HelloWorld(geany.Plugin):

    __plugin_name__ = "HelloWorld"
    __plugin_version__ = "1.0"
    __plugin_description__ = "Just another tool to say hello world"
    __plugin_author__ = "John Doe <john.doe@example.org>"

    def __init__(self):
        self.menu_item = gtk.MenuItem("Hello World")
        self.menu_item.show()
        geany.main_widgets.tools_menu.append(self.menu_item)
        self.menu_item.connect("activate", self.on_hello_item_clicked)

    def cleanup(self):
        self.menu_item.destroy()

    def on_hello_item_clicked(widget, data):
        geany.dialogs.show_msgbox("Hello World")
