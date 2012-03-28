/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __DH_ERROR_H__
#define __DH_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

#define DH_ERROR dh_error_quark ()

typedef enum {
        DH_ERROR_FILE_NOT_FOUND,
        DH_ERROR_MALFORMED_BOOK,
        DH_ERROR_INVALID_BOOK_TYPE,
        DH_ERROR_INTERNAL_ERROR
} DhError;

GQuark dh_error_quark (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __DH_ERROR_H__ */
