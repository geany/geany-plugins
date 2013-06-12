The :mod:`document` module
**************************

.. module:: document
    :synopsis: Document-related functions and classes

This module provides functions for working with documents.  Most of the module-level
functions are used for creating instances of the :class:`Document` object.


.. function:: find_by_filename(filename)

    Finds a document with the given filename from the open documents.

    :param filename: Filename of :class:`Document` to find.

    :return: A :class:`Document` instance for the found document or :data:`None`.

.. function:: get_current()

    Gets the currently active document.

    :return: A :class:`Document` instance for the currently active document or :data:`None` if no documents are open.

.. function:: get_from_page(page_num)

    Gets a document based on it's :class:`gtk.Notebook` page number.

    :param page_num: The tab number of the document in the documents notebook.

    :return: A :class:`Document` instance for the corresponding document or :data:`None` if no document matched.

.. function:: get_from_index(index)

    Gets a document based on its index in Geany's documents array.

    :param index: The index of the document in Geany's documents array.

    :return: A :class:`Document` instance for the corresponding document or :data:`None` if not document matched, or the document that matched isn't valid.

.. function:: new([filename=None[, filetype=None[, text=None]]])

    Creates a document file.

    :param filename: The documents filename, or :data:`None` for `untitled`.
    :param filetype: The documents filetype or :data:`None` to auto-detect it from `filename` (if it's not :data:`None`)
    :param text: Initial text to put in the new document or :data:`None` to leave it blank

    :return: A :class:`Document` instance for the new document.

.. function:: open(filename[, read_only=False[, filetype=None[, forced_enc=None]]])

    Open an existing document file.

    :param filename: Filename of the document to open.
    :param read_only: Whether to open the document in read-only mode.
    :param filetype: Filetype to open the document as or :data:`None` to detect it automatically.
    :param forced_enc: The file encoding to use or :data:`None` to auto-detect it.

    :return: A :class:`Document` instance for the opened document or :data:`None` if it couldn't be opened.

.. function:: open_files(filenames, read_only=False, filetype="", forced_enc="")

    Open multiple files.  This actually calls :func:`open` once for each filename in `filenames`.

    :param filenames: List of filenames to open.
    :param read_only: Whether to open the document in read-only mode.
    :param filetype: Filetype to open the document as or :data:`None` to detect it automatically.
    :param forced_enc: The file encoding to use or :data:`None` to auto-detect it.

.. function:: remove_page(page_num)

    Remove a document from the documents array based on it's page number in the documents notebook.

    :param page_num: The tab number of the document in the documents notebook.

    :return: :data:`True` if the document was actually removed or :data:`False` otherwise.

.. function:: get_documents_list()

    Get a list of open documents.

    :return: A list of :class:`Document` instances, one for each open document.


:class:`Document` Objects
=========================

.. class:: Document

    The main class holding information about a specific document.  Unless
    otherwise noted, the attributes are read-only properties.

    .. attribute:: Document.basename_for_display

        The last part of the filename for this document, possibly truncated to a maximum length in case the filename is very long.

    .. attribute:: Document.notebook_page

        The page number in the :class:`gtk.Notebook` containing documents.

    .. attribute:: Document.status_color

        Gets the status color of the document, or :data:`None` if the default widget coloring should be used.  The color is red if the document has changes, green if it's read-only or :data:`None` if the document is unmodified but writable.  The value is a tuple of the RGB values for red, green, and blue respectively.

    .. attribute:: Document.encoding

        The encoding of this document.  Must be a valid string representation of an encoding.  This property is read-write.

    .. attribute:: Document.file_type

        The file type of this document as a :class:`Filetype` instance.  This property is read-write.

    .. attribute:: Document.text_changed

        Whether this document's text has been changed since it was last saved.

    .. attribute:: Document.file_name

        The file name of this document.

    .. attribute:: Document.has_bom

        Indicates whether the document's file has a byte-order-mark.

    .. attribute:: Document.has_tags

        Indicates whether this document supports source code symbols (tags) to show in the sidebar.

    .. attribute:: Document.index

        Index of the document in Geany's documents array.

    .. attribute:: Document.is_valid

        Indicates whether this document is active and all properties are set correctly.

    .. attribute:: Document.read_only

        Whether the document is in read-only mode.

    .. attribute:: Document.real_path

        The link-dereferenced, locale-encoded file name for this document.

    .. attribute:: Document.editor

        The :class:`Editor` instance associated with this document.

    .. method:: Document.close()

        Close this document.

        :return: :data:`True` if the document was closed, :data:`False` otherwise.

    .. method:: Document.reload([forced_enc=None])

        Reloads this document.

        :param forced_enc: The encoding to use when reloading this document or :data:`None` to auto-detect it.

        :return: :data:`True` if the document was actually reloaded or :data:`False` otherwise.

    .. method:: Document.rename(new_filename)

        Rename this document to a new file name.  Only the file on disk is actually
        renamed, you still have to call :meth:`save_as` to change the document object.
        It also stops monitoring for file changes to prevent receiving too many file
        change events while renaming.  File monitoring is setup again in :meth:`save_as`.

        :param new_filename: The new filename to rename to.

    .. method:: Document.save([force=False])

        Saves this documents file on disk.

        Saving may include replacing tabs by spaces, stripping trailing spaces and adding
        a final new line at the end of the file, depending on user preferences.  Then,
        the `document-before-save` signal is emitted, allowing plugins to modify the
        document before it's saved, and the data is actually written to disk.  The
        file type is set again or auto-detected if it wasn't set yet.  Afterwards,
        the `document-save` signal is emitted for plugins.  If the file is not modified,
        this method does nothing unless `force` is set to :data:`True`.

        **Note:** You should ensure that :attr:`file_name` is not :data:`None` before
        calling this; otherwise call :func:`dialogs.show_save_as`.

        :param force: Whether to save the document even if it's not modified.

        :return: :data:`True` if the file was saved or :data:`False` if the file could not or should not be saved.

    .. method:: Document.save_as(new_filename)

        Saves the document with a new filename, detecting the filetype.

        :param new_filename: The new filename.

        :return: :data:`True` if the file was saved or :data:`False` if it could not be saved.

