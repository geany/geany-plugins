.. -\*- reStructuredText -\*-

# Tweaks-UI Plugin for Geany

This plugin provides (mainly) user-interface tweaks for Geany.

## Features

* Auto Read Only: Automatically switch to read-only mode for files without write permission.
* Column Markers: Show multiple column markers in the editor.
* Detect Filetype: Detect filetype on reload.
* Hide Menubar: Hide the menubar via keybinding.  Restore previous state or hide at startup.
* Sidebar Auto Size: Automatically size the sidebar to keep a specified number of columns visible in the editor on maximize/unmaximize.
* UnchangeDocument: Unsets the change state for new, empty documents.
* WindowGeometry: Save/restore window geometry.  Tries to fix *some* of Geany's glitchy save/restore behavior.  Works regardless of whether Geany "save" position settings are enabled.  Also saves and restores different sidebar and message window positions for maximized and normal window states.

### Ported from the Addons plugin

* ColorTip: Visualize color hex codes in a tooltip.  Open the color chooser tool.
* MarkWord: Highlight double clicked words throughout the document.

### Keybindings

* Extra Copy and Paste
* Toggle menubar visibility
* Toggle document read-only mode
* Redetect filetype.

### Notes

* The main plugin loads settings, registers keybindings, and initializes objects.  Each feature is implemented in a separate object that registers its own signal handlers when initialized.  When not enabled, no signal handlers are registered, and no further processing power is consumed, not even to check whether the feature is enabled.  So even though this plugin has many features that many people may not use, the overall effect on performance is minimal.

* The sidebar management features of WindowGeometry are disabled when Sidebar Auto Size is enabled.  This is mainly for performance because the expected long-term steady state is for Sidebar Auto Size to have its settings applied.

## Usage

Options with their descriptions are contained in the config file `~/.config/geany/plugins/tweaks/tweaks-ui.conf`.  For convenience, menu items under *Tools / Tweaks-UI* may be used to edit and reload the configuration. 

Keybindings are accessible under *Edit / Preferences / Keybindings / Tweaks-UI*.

## Installing

This plugin is not yet available in distro package repositories.  Experimental (aka, *buggy*) versions of this and some other plugins are available for Ubuntu via [PPA](https://launchpad.net/~xiota/+archive/ubuntu/geany-plugins).

## Problems

If you are having problems with geany-plugins, you may report problems at [geany/plugins](https://github.com/geany/plugins/).

If you are using packages from the [PPA](https://launchpad.net/~xiota/+archive/ubuntu/geany-plugins), please report problems at [xiota/geany-tweaks-ui](https://github.com/xiota/geany-tweaks-ui/).

## License

This plugin is licensed under the [GPLv3](COPYING) or later.
