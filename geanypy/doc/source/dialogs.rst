The :mod:`dialogs` module
*************************

.. module:: dialogs
    :synopsis: Showing dialogs to the user

This module contains some help functions to show file-related dialogs,
miscellaneous dialogs, etc.  You can of course just use the :mod:`gtk` module
to create your own dialogs as well.

.. function:: show_input([title=None[, parent=None[, label_text=None[, default_text=None]]]])

    Shows a :class:`gtk.Dialog` to ask the user for text input.

    :param title: The window title for the dialog.
    :param parent: The parent :class:`gtk.Window` for the dialog, for example :data:`geany.main_widgets.window`.
    :param label_text: Text to put in the label just about the input entry box.
    :param default_text: Default text to put in the input entry box.

    :return: A string containing the text the user entered.

.. function:: show_input_numeric([title=None[, label_text=None[, value=0.0[, minimum=0.0[, maximum=100.0[, step=1.0]]]]]])

    Shows a :class:`gtk.Dialog` to ask the user to enter a numeric value
    using a :class:`gtk.SpinButton`.

    :param title: The window title for the dialog.
    :param label_text: Text to put in the label just about the numeric entry box.
    :param value: The initial value in the numeric entry box.
    :param minimum: The minimum allowed value that can be entered.
    :param maximum: The maximum allowed value that can be entered.
    :param step: Amount to increment the numeric entry when it's value is moved up or down (ex, using arrows).

    :return: The value entered if the dialog was closed with ok, or :data:`None` if it was cancelled.

.. function:: show_msgbox(text[, msgtype=gtk.MESSAGE_INFO])

    Shows a :class:`gtk.Dialog` to show the user a message.

    :param text: The text to show in the message box.
    :param msgtype: The message type which is one of the `Gtk Message Type Constants <http://www.pygtk.org/docs/pygtk/gtk-constants.html#gtk-message-type-constants>`_.

.. function:: show_question(text)

    Shows a :class:`gtk.Dialog` to ask the user a Yes/No question.

    :param text: The text to show in the question dialog.

    :return: :data:`True` if the Yes button was pressed, :data:`False` if the No button was pressed.

.. function:: show_save_as()

    Shows Geany's `Save As` dialog.

    :return: :data:`True` if the file was saved, :data:`False` otherwise.
