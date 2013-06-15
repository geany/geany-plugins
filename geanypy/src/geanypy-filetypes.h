#ifndef GEANYPY_FILETYPES_H__
#define GEANYPY_FILETYPES_H__

typedef struct
{
	PyObject_HEAD
	GeanyFiletype *ft;
} Filetype;

Filetype *Filetype_create_new_from_geany_filetype(GeanyFiletype *ft);

#endif /* GEANYPY_FILETYPES_H__ */
