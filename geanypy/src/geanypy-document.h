#ifndef GEANYPY_DOCUMENT_H__
#define GEANYPY_DOCUMENT_H__

typedef struct
{
	PyObject_HEAD
	GeanyDocument *doc;
} Document;

Document *Document_create_new_from_geany_document(GeanyDocument *doc);

#endif /* GEANYPY_DOCUMENT_H__ */
