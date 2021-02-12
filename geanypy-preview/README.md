# Preview

Plugin for Geany that displays a preview of documents written in various markup languages.

![][img-geany]

[img-geany]: README-geany.png


## Introduction

The main purpose of this plugin is to display a preview of documents written in supported markup languages.  Text is processed with a combination of native python, external text processors, and python modules.

 * HTML and plain text are minimally processed.

 * RFC-822-like email-messages are processed according to the `Content-Type` header.

 * Most formats are processed by `pandoc`.
These include Markdown, reStructuredText, Txt2tags, LaTeX, Docbook, Textile, and org-mode.

 * Asciidoc is processed by `asciidoc` or `asciidoctor`.

 * Fountain is processed by the `screenplain` module.

 * RTF is processed by `unrtf`.

Additional features include:

 * The preview panel can be used as a rudimentary web browser.

 * The visibility of the Geany menubar can be toggled to recover some vertical screen space.

 * The generated preview can be saved as an HTML document.

 * Double spaces between sentences are preserved.  Those who prefer to not have double spaces between sentences may refrain from typing them.


## Usage

 * Once installed, the 'Preview' tab will be available in the sidebar.

 * The preview panel can be used as a rudimentary web browser.  Preview is suspended while browsing.  To resume preview, clear the location bar.

 * To toggle menubar visibility, click on the toolbar icon in the Preview tab.

	- To set a keyboard shortcut, go to: Edit | Preferences | Keybindings.

 * For convenience, the Preview window can be scrolled to the approximate caret location in the source document by right clicking the preview window and selecting "Sync Position".  Positioning is not precise, and the Preview may need to be further scrolled up or down to find the desired text.

 * The generated preview can be saved as an HTML document by right clicking and selecting "Save As..."


## Installation

 * Copy `preview.py` to the geany plugins directory (`~/.config/geany/plugins/`).

 * Copy `preview-*.css` to the geany plugins directory.

 * Copy `screenplain/` from [screenplain][sp1] to the geany plugins directory.  An archive of the project is included for convenience.

 * In the menubar, select Tools | Plugin Manager.

   ![][img-plugins]

 * Enable the geanypy and preview plugins.  The preview plugin will appear as a sub-item after geanypy has been enabled.

 * Click "Keybindings", after selecting the "Preview" plugin, to set a keyboard shortcut to toggle menubar visibility.


[img-plugins]: README-plugins.png


## Configuration

In the menubar, select Edit | Plugin Preferences | Preview:

![][img-config-gui]

[img-config-gui]: README-config-gui.png

### General Options

 * **asciidoc processor**.  Which text processor to use for `asciidoc` documents.  Both `asciidoc` and `asciidoctor` are supported because they treat markup slightly differently.  However, `asciidoctor` is preferred because it performs better.

	- `asciidoc`
	- **`asciidoctor`**

 * **default filetype**.  What filetype to process as when the Geany filetype is set to `None`.  May be set to one of the following:

	- **plain**
	- html
	- markdown
	- restructuredtext
	- txt2tags
	- textile
	- org
	- asciidoc
	- fountain

	Note: If set to anything other than `plain` or `html`, Geany may appear to hang while it waits for non-matching file types to be processed.

 * **update interval**.  Limit automatic reprocessing of files to no more than once during this time period (in seconds).  Does not prevent reprocessing when switching documents or saving.

### Appearance Options

 * **hide menubar at startup**.

 * **preview position**.  Where to display the document preview.

	- **sidebar**
	- message window

 * **output position**.  Which tab in the message window to send informational messages.

	- status tab
	- compiler tab
	- **messages tab**
	- standard output (or where ever the standard `print` command sends them)


## Other Customizations

 * CSS files may be customized for each filetype.  They are named `preview-[filetype].css`, located in the plugin directory.  When no other files are available, the default is `preview-html.css`.


## Known problems

 * Rapid reprocessing of files can make geany unusably slow.

	- Increase the update_interval.

 * Setting a high update_interval causes the preview to lag behind the document contents.

	- The preview will be updated, if needed, after update_interval has passed.
	- Immediate reprocessing may be forced by saving or switching documents.

 * Geany may appear to hang while it waits for the text processor to process very large or unusual files.

	- This tends to occur if default_filetype is set to something other than `plain`.
	- This is more likely to occur with slow text processors, like `asciidoc`.  Use `asciidoctor` instead.

 * "Sync Position" is not exact.  The Preview may need to be further scrolled up or down to find the desired text.

	- Let me know if you know of a better way to synchronize the Preview to the source document.

## Requirements

 * geany
 * geanypy
 * python-gtk2
 * python-webkit
 * `pandoc`
 * `asciidoc` or `asciidoctor`
 * `screenplain`


## Licenses

The Preview plugin is licensed under the [GNU General Public License, version 2][pv1].

[Screenplain][sp1] is Copyright 2011 by Martin Vilcans.  It is included/used without modification under the [MIT license][sp2].

[pv1]: http://www.gnu.org/licenses/gpl-2.0.html
[sp1]: https://github.com/vilcans/screenplain/
[sp2]: http://www.opensource.org/licenses/mit-license.php
