#ifndef GEANYPY_ENCODING_H__
#define GEANYPY_ENCODING_H__

PyTypeObject EncodingType;

typedef struct
{
	PyObject_HEAD
	GeanyEncoding *encoding;
} Encoding;

Encoding *Encoding_create_new_from_geany_encoding(GeanyEncoding *enc);

#endif /* GEANYPY_ENCODING_H__ */
