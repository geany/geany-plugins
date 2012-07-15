Geany Markdown Plugin
=====================

The Markdown plugin shows a live preview of Markdown documents in a WebKit
view. The Markdown is converted to HTML and then replaced into a
user-configurable HTML template, the result of which is loaded directly
into the WebKit view.

There are a few settings which can be chosen from the Tools menu and they
will persist between instances of Geany.

Template
--------

This option is accessible from the `Tools->Markdown->Template` menu. Templates
are read from the system-wide configuration directory and then from the user
configuration directory. Templates are just special directories containing
an `index.html` file which defines the template to use for the live preview.

For example, the Default template's `index.html` file consists of the
following contents:

    <html>
      <body>
        <!-- @markdown_document@ -->
      </body>
    </html>

The special HTML comment `<!-- @markdown_document@ -->` is replaced
with the Markdown contents of the editor buffer after the contents have been
converted to HTML. You can put whatever you want in the HTML template, for
example a header and/or footer or some CSS styling.

### Template Paths

User templates are read from Geany's configuration directory (usually
`$HOME/.config/geany/plugins/markdown/templates`. System-wide templates,
including the default `Default` and `Alternate` templates are read from
`$PREFIX/share/geany-plugins/markdown/templates`.

### Custom Templates

To add your own template, just copy one of the existing ones from the
system-wide templates directory into your user configuration directory. The
name of the template is taken from the name of the directory holding the
`index.html`. For example if you wanted the template to be named `FooBar`
then the `index.html` file path would be
`$HOME/.config/geany/plugins/markdown/templates/FooBar/index.html`.

Position
--------

This option specifies which notebook the Markdown preview tab will appear in.
The two choices are `Sidebar` or `Message Window` for Geany's sidebar and
message window notebooks, respectively.

Acknowledgements
----------------
This product includes software developed by
David Loren Parsons <http://www.pell.portland.or.us/~orc>
