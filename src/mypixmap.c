/*      $Id$
 
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.
 
        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
 
        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        XPM load routines taken from GdkPixbuf:
 
        Copyright (C) 1999 Mark Crichton
        Copyright (C) 1999 The Free Software Foundation
 
        Authors: Mark Crichton <crichton@gimp.org>
                 Federico Mena-Quintero <federico@gimp.org>
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2006 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libxfce4util/libxfce4util.h> 
#include <stdlib.h>
#include <stdio.h>

#include "mypixmap.h"
#include "xpm-color-table.h"

enum buf_op 
{
    op_header,
    op_cmap,
    op_body
};

typedef struct 
{
    gchar *color_string;
    guint16 red;
    guint16 green;
    guint16 blue;
    gint transparent;
}
XPMColor;

struct file_handle 
{
    FILE *infile;
    gchar *buffer;
    guint buffer_size;
};

/* The following 2 routines (parse_color, find_color) come from Tk, via the Win32
 * port of GDK. The licensing terms on these (longer than the functions) is:
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The following
 * terms apply to all files associated with the software unless explicitly
 * disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 * 
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal 
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense, the
 * software shall be classified as "Commercial Computer Software" and the
 * Government shall have only "Restricted Rights" as defined in Clause
 * 252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
 * authors grant the U.S. Government and others acting in its behalf
 * permission to use and distribute the software in accordance with the
 * terms specified in this license.
 */
 
static int
compare_xcolor_entries (const void *a, const void *b)
{
    return g_ascii_strcasecmp ((const char *) a, 
                               color_names + ((const XPMColorEntry *) b)->name_offset);
}

static gboolean
find_color(const char *name, XPMColor *colorPtr)
{
    XPMColorEntry *found;

    found = bsearch (name, xColors, G_N_ELEMENTS (xColors), sizeof (XPMColorEntry),
                     compare_xcolor_entries);
    if (found == NULL)
    {
        return FALSE;
    }

    colorPtr->red = (found->red * 65535) / 255;
    colorPtr->green = (found->green * 65535) / 255;
    colorPtr->blue = (found->blue * 65535) / 255;

    return TRUE;
}

static gboolean
parse_color (const char *spec, XPMColor   *colorPtr)
{
    if (spec[0] == '#') 
    {
        char fmt[16];
        int i, red, green, blue;

        if ((i = strlen (spec + 1)) % 3) 
        {
                return FALSE;
        }
        i /= 3;

        g_snprintf (fmt, 16, "%%%dx%%%dx%%%dx", i, i, i);

        if (sscanf (spec + 1, fmt, &red, &green, &blue) != 3)
        {
            return FALSE;
        }
        if (i == 4) 
        {
            colorPtr->red = red;
            colorPtr->green = green;
            colorPtr->blue = blue;
        }
        else if (i == 1) 
        {
            colorPtr->red = (red * 65535) / 15;
            colorPtr->green = (green * 65535) / 15;
            colorPtr->blue = (blue * 65535) / 15;
        }
        else if (i == 2)
        {
            colorPtr->red = (red * 65535) / 255;
            colorPtr->green = (green * 65535) / 255;
            colorPtr->blue = (blue * 65535) / 255;
        }
        else /* if (i == 3) */ 
        {
            colorPtr->red = (red * 65535) / 4095;
            colorPtr->green = (green * 65535) / 4095;
            colorPtr->blue = (blue * 65535) / 4095;
        }
    } 
    else 
    {
        if (!find_color(spec, colorPtr))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static gint
xpm_seek_string (FILE *infile, const gchar *str)
{
    char instr[1024];

    while (!feof (infile)) 
    {
        if (fscanf (infile, "%1023s", instr) < 0)
        {
                return FALSE;
        }
        if (strcmp (instr, str) == 0)
        {
                return TRUE;
        }
    }

    return FALSE;
}

static gint
xpm_seek_char (FILE *infile, gchar c)
{
    gint b, oldb;

    while ((b = getc (infile)) != EOF) 
    {
        if (c != b && b == '/') 
        {
            b = getc (infile);
            if (b == EOF)
            {
                return FALSE;
            }
            else if (b == '*') 
            {   /* we have a comment */
                 b = -1;
                 do 
                 {
                     oldb = b;
                     b = getc (infile);
                     if (b == EOF)
                     {
                             return FALSE;
                     }
                 } 
                 while (!(oldb == '*' && b == '/'));
            }
        } 
        else if (c == b)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static gint
xpm_read_string (FILE *infile, gchar **buffer, guint *buffer_size)
{
    gint c;
    guint cnt = 0, bufsiz, ret = FALSE;
    gchar *buf;

    buf = *buffer;
    bufsiz = *buffer_size;
    if (buf == NULL) 
    {
        bufsiz = 10 * sizeof (gchar);
        buf = g_new (gchar, bufsiz);
    }

    do 
    {
        c = getc (infile);
    } 
    while (c != EOF && c != '"');

    if (c != '"')
    {
        goto out;
    }
    while ((c = getc (infile)) != EOF) 
    {
        if (cnt == bufsiz) 
        {
            guint new_size = bufsiz * 2;

            if (new_size > bufsiz)
            {
                bufsiz = new_size;
            }
            else
            {
                goto out;
            }
            buf = g_realloc (buf, bufsiz);
            buf[bufsiz - 1] = '\0';
        }

        if (c != '"')
        {
            buf[cnt++] = c;
        }
        else 
        {
            buf[cnt] = 0;
            ret = TRUE;
            break;
        }
    }

out:
    buf[bufsiz - 1] = '\0';     /* ensure null termination for errors */
    *buffer = buf;
    *buffer_size = bufsiz;
    return ret;
}

static gchar *
search_color_symbol (gchar *symbol, xfwmColorSymbol *color_sym)
{
    xfwmColorSymbol *i;
    
    i = color_sym;
    while (i && i->name)
    {
        if (!g_ascii_strcasecmp (i->name, symbol))
        {
            return i->value;
        }
        ++i;
    }
    return NULL;
}

static gchar *
xpm_extract_color (const gchar *buffer, xfwmColorSymbol *color_sym)
{
    const gchar *p = &buffer[0];
    gint new_key = 0;
    gint key = 0;
    gint current_key = 1;
    gint space = 128;
    gchar word[129], color[129], current_color[129];
    gchar *r; 

    word[0] = '\0';
    color[0] = '\0';
    current_color[0] = '\0';
    while (1) 
    {
        /* skip whitespace */
        for (; *p != '\0' && g_ascii_isspace (*p); p++) 
        {
        } 
        /* copy word */
        for (r = word; 
                 (*p != '\0') && 
                 (!g_ascii_isspace (*p)) && 
                 (r - word < sizeof (word) - 1); 
             p++, r++) 
        {
                *r = *p;
        }
        *r = '\0';
        if (*word == '\0') 
        {
            if (color[0] == '\0')  /* incomplete colormap entry */
            {
                return NULL;                            
            }
            else  /* end of entry, still store the last color */
            {
                new_key = 1;
            }
        }
        else if (key > 0 && color[0] == '\0')  /* next word must be a color name part */
        {
                new_key = 0;
        } 
        else 
        {
            if (strcmp (word, "s") == 0)
            {
                new_key = 5;
            }
            else if (strcmp (word, "c") == 0)
            {
                new_key = 4;
            }
            else if (strcmp (word, "g") == 0)
            {
                new_key = 3;
            }
            else if (strcmp (word, "g4") == 0)
            {
                new_key = 2;
            }
            else if (strcmp (word, "m") == 0)
            {
                new_key = 1;
            }
            else 
            {
                new_key = 0;
            }
        }
        if (new_key == 0) 
        {  /* word is a color name part */
            if (key == 0)  /* key expected */
            {
                return NULL;
            }
            /* accumulate color name */
            if (color[0] != '\0') 
            {
                strncat (color, " ", space);
                space -= MIN (space, 1);
            }
            strncat (color, word, space);
            space -= MIN (space, strlen (word));
        }
        else if (key == 5)
        {
            gchar *new_color = NULL;
            new_color = search_color_symbol (color, color_sym);
            if (new_color)
            {
                current_key = key;
                strcpy (current_color, new_color);
            }
            space = 128;
            color[0] = '\0';
            key = new_key;
            if (*p == '\0') 
            {
                break;
            }
        }
        else
        {  /* word is a key */
            if (key > current_key) 
            {
                current_key = key;
                strcpy (current_color, color);
            }
            space = 128;
            color[0] = '\0';
            key = new_key;
            if (*p == '\0') 
            {
                break;
            }
        }
    }
    if (current_key > 1)
    {
        return g_strdup (current_color);
    }
    else
    {
        return NULL; 
    }
}

static const gchar *
file_buffer (enum buf_op op, gpointer handle)
{
    struct file_handle *h = handle;

    switch (op) 
    {
        case op_header:
            if (xpm_seek_string (h->infile, "XPM") != TRUE)
            {
                break;
            }
            if (xpm_seek_char (h->infile, '{') != TRUE)
            {
                break;
            }
            /* Fall through to the next xpm_seek_char. */

        case op_cmap:
            xpm_seek_char (h->infile, '"');
            fseek (h->infile, -1, SEEK_CUR);
            /* Fall through to the xpm_read_string. */

        case op_body:
            if(!xpm_read_string (h->infile, &h->buffer, &h->buffer_size))
            {
                return NULL;
            }
            return h->buffer;

        default:
            g_assert_not_reached ();
    }

    return NULL;
}

/* This function does all the work. */
static GdkPixbuf *
pixbuf_create_from_xpm (const gchar * (*get_buf) (enum buf_op op, gpointer handle), gpointer handle, xfwmColorSymbol *color_sym)
{
    gint w, h, n_col, cpp, x_hot, y_hot, items;
    gint cnt, xcnt, ycnt, wbytes, n;
    gint is_trans = FALSE;
    const gchar *buffer;
    gchar *name_buf;
    gchar pixel_str[32];
    GHashTable *color_hash;
    XPMColor *colors, *color, *fallbackcolor;
    guchar *pixtmp;
    GdkPixbuf *pixbuf;

    fallbackcolor = NULL;

    buffer = (*get_buf) (op_header, handle);
    if (!buffer) 
    {
        return NULL;
    }
    items = sscanf (buffer, "%d %d %d %d %d %d", &w, &h, &n_col, &cpp, &x_hot, &y_hot);

    if (items != 4 && items != 6) 
    {
        return NULL;
    }

    if ((w <= 0) || 
        (h <= 0) || 
        (cpp <= 0) || 
        (cpp >= 32) || 
        (n_col <= 0) || 
        (n_col >= G_MAXINT / (cpp + 1)) || 
        (n_col >= G_MAXINT / sizeof (XPMColor)))
    {
        return NULL;
    }

    /* The hash is used for fast lookups of color from chars */
    color_hash = g_hash_table_new (g_str_hash, g_str_equal);

    name_buf = g_try_malloc (n_col * (cpp + 1));
    if (!name_buf) {
        g_hash_table_destroy (color_hash);
        return NULL;
    }

    colors = (XPMColor *) g_try_malloc (sizeof (XPMColor) * n_col);
    if (!colors) {
        g_hash_table_destroy (color_hash);
        g_free (name_buf);
        return NULL;
    }

    for (cnt = 0; cnt < n_col; cnt++) 
    {
        gchar *color_name;

        buffer = (*get_buf) (op_cmap, handle);
        if (!buffer) 
        {
            g_hash_table_destroy (color_hash);
            g_free (name_buf);
            g_free (colors);
            return NULL;
        }

        color = &colors[cnt];
        color->color_string = &name_buf[cnt * (cpp + 1)];
        strncpy (color->color_string, buffer, cpp);
        color->color_string[cpp] = 0;
        buffer += strlen (color->color_string);
        color->transparent = FALSE;

        color_name = xpm_extract_color (buffer, color_sym);

        if ((color_name == NULL) || 
            (g_ascii_strcasecmp (color_name, "None") == 0) ||
            (parse_color (color_name, color) == FALSE)) 
        {
            color->transparent = TRUE;
            color->red = 0;
            color->green = 0;
            color->blue = 0;
            is_trans = TRUE;
        }

        g_free (color_name);
        g_hash_table_insert (color_hash, color->color_string, color);

        if (cnt == 0)
        {
            fallbackcolor = color;
        }
    }

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, is_trans, 8, w, h);

    if (!pixbuf) 
    {
        g_hash_table_destroy (color_hash);
        g_free (colors);
        g_free (name_buf);
        return NULL;
    }

    wbytes = w * cpp;

    for (ycnt = 0; ycnt < h; ycnt++) 
    {
        pixtmp = gdk_pixbuf_get_pixels (pixbuf) + ycnt * gdk_pixbuf_get_rowstride(pixbuf);

        buffer = (*get_buf) (op_body, handle);
        if ((!buffer) || (strlen (buffer) < wbytes))
        {
            continue;
        }

        for (n = 0, cnt = 0, xcnt = 0; n < wbytes; n += cpp, xcnt++) 
        {
            strncpy (pixel_str, &buffer[n], cpp);
            pixel_str[cpp] = 0;

            color = g_hash_table_lookup (color_hash, pixel_str);

            /* Bad XPM...punt */
            if (!color)
            {
                color = fallbackcolor;
            }

            *pixtmp++ = color->red >> 8;
            *pixtmp++ = color->green >> 8;
            *pixtmp++ = color->blue >> 8;

            if (is_trans && color->transparent)
            {
                *pixtmp++ = 0;
            }
            else if (is_trans)
            {
                *pixtmp++ = 0xFF;
            }
        }
    }

    g_hash_table_destroy (color_hash);
    g_free (colors);
    g_free (name_buf);

    if (items == 6) 
    {
        gchar hot[10];
        g_snprintf (hot, 10, "%d", x_hot);
        gdk_pixbuf_set_option (pixbuf, "x_hot", hot);
        g_snprintf (hot, 10, "%d", y_hot);
        gdk_pixbuf_set_option (pixbuf, "y_hot", hot);
    }

    return pixbuf;
}


static GdkPixbuf *
xpm_image_load (const char *filename, xfwmColorSymbol *color_sym)
{
    guchar buffer[1024];
    GdkPixbuf *pixbuf;
    struct file_handle h;
    int size;
    FILE *f;

    f = g_fopen (filename, "rb");
    if (!f) 
    {
        return NULL;
    }

    size = fread (&buffer, 1, sizeof (buffer), f);
    if (size == 0) 
    {
        fclose (f);
        return NULL;
    }

    fseek (f, 0, SEEK_SET);
    memset (&h, 0, sizeof (h));
    h.infile = f;
    pixbuf = pixbuf_create_from_xpm (file_buffer, &h, color_sym);
    g_free (h.buffer);
    fclose (f);

    return pixbuf;
}

static void
xfwmPixmapRefreshPict (xfwmPixmap * pm)
{
#ifdef HAVE_RENDER
    ScreenInfo * screen_info = pm->screen_info;

    if (!pm->pict_format)
    {
        pm->pict_format = XRenderFindVisualFormat (myScreenGetXDisplay (screen_info), 
                                                   screen_info->visual);
    }

    if (pm->pict != None)
    {
        XRenderFreePicture (myScreenGetXDisplay(pm->screen_info), pm->pict);
        pm->pict = None;
    }

    if ((pm->pixmap) && (pm->pict_format))
    {
        pm->pict = XRenderCreatePicture (myScreenGetXDisplay (screen_info), 
                                         pm->pixmap, pm->pict_format, 0, NULL);
    }
#endif
}

static gboolean
xfwmPixmapCompose (GdkPixbuf *pixbuf, gchar * dir, gchar * file)
{
    gchar *filepng;
    gchar *filename;
    GdkPixbuf *alpha;
    GError *error = NULL;
    gint width, height;
    gboolean status;

    filepng = g_strdup_printf ("%s.%s", file, "png");
    filename = g_build_filename (dir, filepng, NULL);
    g_free (filepng);

    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    {
        g_free (filename);
        return FALSE;
    }

    alpha = gdk_pixbuf_new_from_file (filename, &error);
    g_free (filename);
    if (error)
    {
        g_warning ("%s", error->message);
        g_error_free (error);
        return FALSE;
    }

    if (!gdk_pixbuf_get_has_alpha (alpha))
    {
        g_object_unref (alpha);
        return FALSE;
    }

    width  = MIN (gdk_pixbuf_get_width (pixbuf), 
                  gdk_pixbuf_get_width (alpha));
    height = MIN (gdk_pixbuf_get_height (pixbuf), 
                  gdk_pixbuf_get_height (alpha));

    gdk_pixbuf_composite (alpha, pixbuf, 0, 0, width, height,
                          0, 0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);

    g_object_unref (alpha);

    return TRUE;
}

static gboolean
xfwmPixmapDrawFromGdkPixbuf (xfwmPixmap * pm, GdkPixbuf *pixbuf)
{
    GdkPixmap *dest_pixmap;
    GdkPixmap *dest_bitmap;
    GdkVisual *gvisual;
    GdkColormap *cmap;
    gint width, height;
    gint dest_x, dest_y;

    dest_pixmap = gdk_xid_table_lookup (pm->pixmap);
    if (dest_pixmap)
    {
        g_object_ref (G_OBJECT (dest_pixmap));
    }
    else
    {
        dest_pixmap = gdk_pixmap_foreign_new (pm->pixmap);
    }
    
    if (!dest_pixmap)
    {
        g_warning ("Cannot get pixmap");
        return FALSE;
    }

    dest_bitmap = gdk_xid_table_lookup (pm->mask);
    if (dest_bitmap)
    {
        g_object_ref (G_OBJECT (dest_bitmap));
    }
    else
    {
        dest_bitmap = gdk_pixmap_foreign_new (pm->mask);
    }
    
    if (!dest_bitmap)
    {
        g_warning ("Cannot get bitmap");
        g_object_unref (dest_pixmap);
        return FALSE;
    }

    gvisual = gdk_screen_get_system_visual (pm->screen_info->gscr);
    cmap = gdk_x11_colormap_foreign_new (gvisual, pm->screen_info->cmap);    

    if (!cmap)
    {
        g_warning ("Cannot create colormap");
        g_object_unref (dest_pixmap);
        g_object_unref (dest_bitmap);
        return FALSE;
    }

    width = MIN (gdk_pixbuf_get_width (pixbuf), pm->width);
    height = MIN (gdk_pixbuf_get_height (pixbuf), pm->height);
    dest_x = (pm->width - width) / 2;
    dest_y = (pm->height - height) / 2;

    gdk_drawable_set_colormap (GDK_DRAWABLE (dest_pixmap), cmap);
    gdk_draw_pixbuf (GDK_DRAWABLE (dest_pixmap), NULL, pixbuf, 0, 0, dest_x, dest_y,
                     width, height, GDK_RGB_DITHER_NONE, 0, 0);

    gdk_pixbuf_render_threshold_alpha (pixbuf, dest_bitmap,
                                       0, 0, dest_x, dest_y,
                                       width, height, 255);

    g_object_unref (cmap);
    g_object_unref (dest_pixmap);
    g_object_unref (dest_bitmap);

    return TRUE;
}

gboolean
xfwmPixmapRenderGdkPixbuf (xfwmPixmap * pm, GdkPixbuf *pixbuf)
{
    GdkPixbuf *src;
    GdkPixmap *destw;
    GdkVisual *gvisual;
    GdkColormap *cmap;
    gint width, height;
    gint dest_x, dest_y;

    destw = gdk_xid_table_lookup (pm->pixmap);
    if (destw)
    {
        g_object_ref (G_OBJECT (destw));
    }
    else
    {
         destw = gdk_pixmap_foreign_new (pm->pixmap);
    }
    
    if (!destw)
    {
        g_warning ("Cannot get pixmap");
        return FALSE;
    }

    gvisual = gdk_screen_get_system_visual (pm->screen_info->gscr);
    cmap = gdk_x11_colormap_foreign_new (gvisual, pm->screen_info->cmap);    

    if (!cmap)
    {
        g_warning ("Cannot create colormap");
        g_object_unref (destw);
        return FALSE;
    }

    width = MIN (gdk_pixbuf_get_width (pixbuf), pm->width);
    height = MIN (gdk_pixbuf_get_height (pixbuf), pm->height);
    dest_x = (pm->width - width) / 2;
    dest_y = (pm->height - height) / 2;

    src = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (destw), cmap, 
                                        dest_x, dest_y, 0, 0, width, height);
    gdk_pixbuf_composite (pixbuf, src, 0, 0, width, height,
                          0, 0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
    gdk_draw_pixbuf (GDK_DRAWABLE (destw), NULL, src, 0, 0, dest_x, dest_y,
                     width, height, GDK_RGB_DITHER_NONE, 0, 0);

    g_object_unref (cmap);
    g_object_unref (src);
    g_object_unref (destw);

    return TRUE;
}

gboolean
xfwmPixmapLoad (ScreenInfo * screen_info, xfwmPixmap * pm, gchar * dir, gchar * file, xfwmColorSymbol * cs)
{
    gchar *filename;
    gchar *filexpm;
    GdkPixbuf *pixbuf;

    TRACE ("entering xfwmPixmapLoad");

    g_return_val_if_fail (dir != NULL, FALSE);
    g_return_val_if_fail (file != NULL, FALSE);

    pm->screen_info = screen_info;
    pm->pixmap = None;
    pm->mask = None;
    pm->width = 1;
    pm->height = 1;
    filexpm = g_strdup_printf ("%s.%s", file, "xpm");
    filename = g_build_filename (dir, filexpm, NULL);
    g_free (filexpm);

    pixbuf = xpm_image_load (filename, cs);
    if (!pixbuf)
    {
        TRACE ("%s not found", filename);
        g_free (filename);
        return FALSE;
    }
    g_free (filename);

    /* Apply the alpha channel from PNG if available */
    xfwmPixmapCompose (pixbuf, dir, file);

    xfwmPixmapCreate (screen_info, pm, 
                      gdk_pixbuf_get_width (pixbuf),
                      gdk_pixbuf_get_height (pixbuf));
    xfwmPixmapDrawFromGdkPixbuf (pm, pixbuf);

#ifdef HAVE_RENDER
    xfwmPixmapRefreshPict (pm);
#endif

    return TRUE;
}

void
xfwmPixmapCreate (ScreenInfo * screen_info, xfwmPixmap * pm, 
                  gint width, gint height)
{
    TRACE ("entering xfwmPixmapCreate, width=%i, height=%i", width, height);
    g_return_if_fail (screen_info != NULL);

    if ((width < 1) || (height < 1))
    {
        xfwmPixmapInit (screen_info, pm);
    }
    else
    {
        pm->screen_info = screen_info;
        pm->pixmap = XCreatePixmap (myScreenGetXDisplay (screen_info), 
                                    screen_info->xroot, 
                                    width, height, screen_info->depth);
        pm->mask = XCreatePixmap (myScreenGetXDisplay (screen_info), 
                                  pm->pixmap, width, height, 1);
        pm->width = width;
        pm->height = height;
#ifdef HAVE_RENDER
        pm->pict_format = XRenderFindVisualFormat (myScreenGetXDisplay (screen_info), 
                                                   screen_info->visual);
        pm->pict = None;
#endif
    }
}

void
xfwmPixmapInit (ScreenInfo * screen_info, xfwmPixmap * pm)
{
    pm->screen_info = screen_info;
    pm->pixmap = None;
    pm->mask = None;
    pm->width = 0;
    pm->height = 0;
#ifdef HAVE_RENDER
    pm->pict_format = XRenderFindVisualFormat (myScreenGetXDisplay (screen_info), 
                                               screen_info->visual);
    pm->pict = None;
#endif
}

void
xfwmPixmapFree (xfwmPixmap * pm)
{
    
    TRACE ("entering xfwmPixmapFree");
    
    if (pm->pixmap != None)
    {
        XFreePixmap (myScreenGetXDisplay(pm->screen_info), pm->pixmap);
        pm->pixmap = None;
    }
    if (pm->mask != None)
    {
        XFreePixmap (myScreenGetXDisplay(pm->screen_info), pm->mask);
        pm->mask = None;
    }
#ifdef HAVE_RENDER
    if (pm->pict != None)
    {
        XRenderFreePicture (myScreenGetXDisplay(pm->screen_info), pm->pict);
        pm->pict = None;
    }
#endif
}

static void
xfwmPixmapFillRectangle (Display *dpy, int screen, Pixmap pm, Drawable d, 
                         int x, int y, int width, int height)
{
    XGCValues gv;
    GC gc;
    unsigned long mask;

    TRACE ("entering fillRectangle");

    if ((width < 1) || (height < 1))
    {
        return;
    }
    gv.fill_style = FillTiled;
    gv.tile = pm;
    gv.ts_x_origin = x;
    gv.ts_y_origin = y;
    gv.foreground = WhitePixel (dpy, screen);
    if (gv.tile != None)
    {
        mask = GCTile | GCFillStyle | GCTileStipXOrigin;
    }
    else
    {
        mask = GCForeground;
    }
    gc = XCreateGC (dpy, d, mask, &gv);
    XFillRectangle (dpy, d, gc, x, y, width, height);
    XFreeGC (dpy, gc);
}

void
xfwmPixmapFill (xfwmPixmap * src, xfwmPixmap * dst, 
                gint x, gint y, gint width, gint height)
{
    TRACE ("entering xfwmWindowFill");

    if ((width < 1) || (height < 1))
    {
        return;
    }

    xfwmPixmapFillRectangle (myScreenGetXDisplay (src->screen_info), 
                             src->screen_info->screen,  
                             src->pixmap, dst->pixmap, x, y, width, height);
    xfwmPixmapFillRectangle (myScreenGetXDisplay (src->screen_info), 
                             src->screen_info->screen,  
                             src->mask, dst->mask, x, y, width, height);
#ifdef HAVE_RENDER
    xfwmPixmapRefreshPict (dst);
#endif
}

void
xfwmPixmapDuplicate (xfwmPixmap * src, xfwmPixmap * dst)
{
    g_return_if_fail (src != NULL);
    TRACE ("entering xfwmPixmapDuplicate, width=%i, height=%i", src->width, src->height);

    xfwmPixmapCreate (src->screen_info, dst, src->width, src->height);
    xfwmPixmapFill (src, dst, 0, 0, src->width, src->height);
}
