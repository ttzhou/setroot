/*****************************************************************************
 *
 *  Copyright (c) 2014-2016 Tim Zhou <ttzhou@uwaterloo.ca>
 *
 *  set_pixmap_property() is (c) 1998 Michael Jennings <mej@eterm.org>
 *
 *  find_desktop() is a modification of: get_desktop_window() (c) 2004-2012
 *  Jonathan Koren <jonathan@jonathankoren.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#define _POSIX_C_SOURCE 200809L // for getline
#define _DEFAULT_SOURCE // for realpath

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef HAVE_LIBXINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <Imlib2.h>

#include "classes.h"
#include "functions.h"
#include "util.h"

/* globals */
Display            *XDPY;
unsigned int        XSCRN_NUM;
Screen             *XSCRN;
Window              ROOT_WIN;
int                 BITDEPTH;
Colormap            COLORMAP;
Visual             *VISUAL;

struct screen       *SCREEN  = NULL;
unsigned int        NUM_MONS = 0;

/* runtime variables */
static       char  *program_name = "setroot";
static const char  *BLANK_COLOR  = "black";

static const int    IMLIB_CACHE_SIZE  = 10240*1024; // I like big walls
static const int    IMLIB_COLOR_USAGE = 256;
static const int    DITHER_SWITCH     = 0;

static const int    SORT_BY_XINM = 0;
static const int    SORT_BY_XORG = 1;
static const int    SORT_BY_YORG = 2;

/*****************************************************************************
 *
 *  set_pixmap_property() is (c) 1998 Michael Jennings <mej@eterm.org>
 *
 *  Original idea goes to Nat Friedman; his email is <ndf@mit.edu>. This is
 *  according to Michael Jennings, who published this program on the
 *  Enlightenment website. See http://www.eterm.org/docs/view.php?doc=ref.
 *
 *  The following function is taken from esetroot.c, which was a way for Eterm,
 *  a part of the Enlightenment project, to implement pseudotransparency.
 *
 *  I do not take credit for any of this code nor do I intend to use it in any
 *  way for self-gain.
 *
 *  For explanation, see http://www.eterm.org/docs/view.php?doc=ref and read
 *  section titled "Transparency".
 *
 *  This is used if you use a compositor that looks for _XROOTPMAP_ID.
 *
 *****************************************************************************/

void
set_pixmap_property(Pixmap p)
{
    Atom prop_root, prop_esetroot, type;
    int format;
    unsigned long length, after;
    unsigned char *data_root, *data_esetroot;

    prop_root = XInternAtom(XDPY, "_XROOTPMAP_ID", True);
    prop_esetroot = XInternAtom(XDPY, "ESETROOTPMAP_ID", True);

    if ((prop_root != None) && (prop_esetroot != None)) {
        XGetWindowProperty(XDPY, ROOT_WIN, prop_root, 0L, 1L, False,
                           AnyPropertyType, &type, &format,
                           &length, &after, &data_root);

        if (type == XA_PIXMAP) {
            XGetWindowProperty(XDPY, ROOT_WIN, prop_esetroot, 0L, 1L,
                               False, AnyPropertyType, &type, &format,
                               &length, &after, &data_esetroot);

            if (data_root && data_esetroot)
                if ((type == XA_PIXMAP) &&
                    (*((Pixmap *) data_root) == *((Pixmap *) data_esetroot)))

                    XKillClient(XDPY, *((Pixmap *) data_root));

            if (data_esetroot) { free(data_esetroot); }
        }
        if (data_root) { free(data_root); }
    }

    prop_root = XInternAtom(XDPY, "_XROOTPMAP_ID", False);
    prop_esetroot = XInternAtom(XDPY, "ESETROOTPMAP_ID", False);

    if (prop_root == None || prop_esetroot == None)
        die(1, "creation of pixmap property failed");

    XChangeProperty(XDPY, ROOT_WIN, prop_root, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char *) &p, 1);
    XChangeProperty(XDPY, ROOT_WIN, prop_esetroot, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char *) &p, 1);

    XSetCloseDownMode(XDPY, RetainPermanent);
    XFlush(XDPY);
}

/*****************************************************************************
 *
 *  find_desktop() is a slight modification of: get_desktop_window()
 *  which is (c) 2004-2012 Jonathan Koren <jonathan@jonathankoren.com>
 *
 *  find_desktop() finds the window that draws the desktop and sets wallpaper.
 *  This is mainly for those weird DEs that don't draw backgrounds
 *  to root window.
 *
 *  The original code was taken from imlibsetroot, by Jonathan Koren.
 *  His email is <jonathan@jonathankoren.com>.
 *
 *  We call this on the root window to start, in main.
 *
 *****************************************************************************/

Window
find_desktop( Window window )
{
    Atom prop_desktop, prop_type;

    Atom ret_type;
    int ret_format;
    unsigned long n_ret, ret_bytes_left;
    Atom *ret_props = NULL;

    Atom prop;

    int has_property = 0;
    int has_children = 0;
    unsigned int n_children = 0;

    Window root;
    Window parent;
    Window child;
    Window *children;

    prop_type = XInternAtom(XDPY, "_NET_WM_WINDOW_TYPE", True);
    prop_desktop = XInternAtom(XDPY, "_NET_WM_WINDOW_TYPE_DESKTOP", True);

    if (prop_type == None) {
        fprintf(stderr, "Could not find window with _NET_WM_WINDOW_TYPE set. "
                        "Defaulting to root window.\n");
        return None;
    }

    // check current window
    has_property = XGetWindowProperty(XDPY, window, prop_type,
                                      0L, sizeof(Atom),
                                      False,
                                      XA_ATOM,
                                      &ret_type,
                                      &ret_format,
                                      &n_ret, &ret_bytes_left,
                                      (unsigned char **) &ret_props);

    if (has_property == Success &&
        ret_props && n_ret > 0 && ret_type == XA_ATOM) {

        for (unsigned int i = 0; i < n_ret; i++) {
            prop = ret_props[i];
            if (prop == prop_desktop) {
                XFree(ret_props);
                return window;
            }
        } XFree(ret_props);
    }
    // otherwise, recursively check children; first call XQueryTree
    has_children = XQueryTree(XDPY, window,
                              &root, &parent,
                              &children, &n_children);

    if (!has_children) {
        fprintf(stderr, "XQueryTree() failed!\n");
        exit(1);
    }

    for (unsigned int i = 0; i < n_children; i++) {
        child = find_desktop(children[i]);
        if (child == None)
            continue;

        return child;
    } if (n_children) XFree(children);

    return None;
}

void
store_call( int argc, char** args )
{
    /*CREATE THE DIRECTORY AND FILE*/
    char *cfg_dir,*fn, *fullpath;
    unsigned int buflen;

    if (getenv("XDG_CONFIG_HOME") == NULL) {
        buflen = strlen(getenv("HOME")) + strlen("/.config/setroot") + 1;
        cfg_dir = malloc(buflen); verify(cfg_dir);
        snprintf(cfg_dir, buflen, "%s/%s/%s", getenv("HOME"),
                                              ".config",
                                              "setroot");
    } else {
        buflen = strlen(getenv("XDG_CONFIG_HOME")) + strlen("/setroot") + 1;
        cfg_dir = malloc(buflen); verify(cfg_dir);
        snprintf(cfg_dir, buflen, "%s/%s", getenv("XDG_CONFIG_HOME"),
                                                  "setroot");
    }
    /*CREATE DIRECTORY*/
    if (mkdir(cfg_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1
        && errno != EEXIST) {

        fprintf(stderr, "Could not create directory %s.\n", cfg_dir);
        exit(1);
    }
    buflen += strlen(".setroot-restore") + 1;
    fn = malloc(buflen); verify(fn);
    snprintf(fn, buflen, "%s/%s", cfg_dir, ".setroot-restore");

    /*WRITE TO THE FILE*/
    FILE *f = fopen(fn, "w");
    if (!f) {
        fprintf(stderr, "Could not write to file %s.\n", fn);
        exit(1);
    }

    fprintf(f, "setroot"); // the initial call
    for (int i = 2; i < argc; i++) { // jump past --store
        fullpath = realpath(args[i], NULL);

        if (fullpath == NULL) {
            fprintf(f, " \'%s\'", args[i]);
            continue;
        }
        unsigned int pathlen = strlen(fullpath);
        /* if does not contain a period, or it does but ends in a digit, */
        /* then assume it is a flag or a flag option */
        if ((strpbrk(fullpath, ".") == NULL) || isdigit(fullpath[pathlen - 1]))
            fprintf(f, " \'%s\'", args[i]);
        else
            fprintf(f, " \'%s\'", fullpath);
        free(fullpath);
    } fclose(f);

    /*GIVE FILE PROPER PERMISSIONS*/
    if (chmod(fn, S_IRWXU | S_IRGRP | S_IROTH | S_IXOTH) != 0) {
        fprintf(stderr, "Could not make file \'%s\' executable.\n", fn);
        exit(1);
    } free(fn); free(cfg_dir);
}

void
restore()
{
    char *fn;
    unsigned int dirlen;

    if (getenv("XDG_CONFIG_HOME") == NULL) {
        dirlen = (strlen(getenv("HOME")) \
                + strlen("/.config/setroot/.setroot-restore") + 1);
        fn = malloc(dirlen); verify(fn);
        snprintf(fn, dirlen, "%s/%s/%s/%s", \
                 getenv("HOME"), ".config", "setroot", ".setroot-restore");
    } else {
        dirlen = (strlen(getenv("XDG_CONFIG_HOME")) \
                + strlen("/setroot/.setroot-restore") + 1);
        fn = malloc(dirlen); verify(fn);
        snprintf(fn, dirlen, "%s/%s/%s", \
                    getenv("XDG_CONFIG_HOME"), "setroot", ".setroot-restore");
    }
    if (system(fn) != 0) {
        fprintf(stderr, "Could not restore wallpaper. Check file %s.\n", fn);
        exit(127);
    } free(fn);
}

struct screen
*init_screen( unsigned int sw, unsigned int sh )
{
    struct screen *s = malloc(sizeof(struct screen));
    verify(s);

    unsigned int nm = 1;

#ifdef HAVE_LIBXINERAMA
    XineramaScreenInfo *XSI = XineramaQueryScreens(XDPY, (int *) &nm);

    if (nm == 0) {
        fprintf(stderr, "Problem detecting Xinerama screens!"
                        "Exiting program!\n");
        exit(1);
    }
#endif

    s->num_mons      = nm;
    s->screen_width  = sw;
    s->screen_height = sh;

    s->monitors = (struct monitor **) malloc(nm*sizeof(struct monitor *));
    verify(s->monitors);

#ifdef HAVE_LIBXINERAMA
    unsigned int i;
    struct monitor *m = NULL;

    for (i = 0; i < nm; i++) {
        m = init_monitor(XSI[i].width,
                         XSI[i].height,
                         XSI[i].x_org,
                         XSI[i].y_org);

        (s->monitors)[i] = m;
    } XFree(XSI);
#else
    (s->monitors)[0] = init_monitor(sw, sh, 0, 0);
#endif
    XSync(XDPY, False);
    return s;
}

struct monitor
*init_monitor( unsigned int w, unsigned int h,
               unsigned int xp, unsigned int yp )
{
    struct monitor *m = malloc(sizeof(struct monitor));
    verify(m);

    m->width  = w;
    m->height = h;

    m->xpos = xp;
    m->ypos = yp;

    m->wall = NULL;

    return m;
}

struct wallpaper
*init_wall()
{
    struct wallpaper *w = malloc(sizeof(struct wallpaper));
    verify(w);

    w->height     = w->width    = 0;
    w->xpos       = w->ypos     = 0; // relative to monitor!
    w->span       = w->monitor  = 0;

    w->xg_type    = PERCENTAGE;
    w->yg_type    = PERCENTAGE;

    w->xg         = 0.5;
    w->yg         = 0.5;

    w->option     = FIT_AUTO;
    w->axis       = NONE;

    w->blur       = w->sharpen  = 0;
    w->brightness = 0;
    w->contrast   = 1.0;
    w->greyscale  = 0;

    w->bgcol      = NULL;
    w->tint       = NULL;
    w->image      = NULL;

    return w;
}

void
clean_screen( struct screen *s )
{
    unsigned int i;
    unsigned int nm = s->num_mons;

    if (s->monitors == NULL || nm == 0) return;

    for (i = 0; i < nm; i++) {
        clean_monitor((s->monitors)[i]);
        free((s->monitors)[i]);
    } free(s->monitors);

    s->monitors = NULL;
}

void
clean_monitor( struct monitor *m )
{
    if (m->wall == NULL) return;

    clean_wall(m->wall);
    free(m->wall);
    m->wall = NULL;
}

void
clean_wall( struct wallpaper *w )
{
    if (w->bgcol != NULL) {
        free(w->bgcol);
        w->bgcol = NULL;
    }
    if (w->tint != NULL) {
        free(w->tint);
        w->tint = NULL;
    }
    if (w->image != NULL) {
        imlib_context_set_image(w->image);
        imlib_free_image_and_decache();
        w->image = NULL;
    }
}

#ifdef HAVE_LIBXINERAMA
void sort_mons_by( struct screen *s, int sort_opt )
{
    unsigned int i;
    unsigned int nm = s->num_mons;

    struct pair values[nm];
    struct monitor *new_order[nm];

    if        (sort_opt == SORT_BY_XORG) {
        for   ( i = 0; i < nm; i++ ) {
            values[i].value = (s->monitors)[i]->xpos;
            values[i].index = i;
        }
        qsort(values, nm, sizeof(struct pair), ascending);
    } else if (sort_opt == SORT_BY_YORG) {
        for ( i = 0; i < nm; i++ ) {
            values[i].value = (s->monitors)[i]->ypos;
            values[i].index = i;
        }
        qsort(values, nm, sizeof(struct pair), descending);
    } else {
        for ( i = 0; i < nm; i++ ) {
            values[i].value = 0;
            values[i].index = i;
        }
    }

    for ( i = 0 ; i < nm ; i++ )
        new_order[i] = (s->monitors)[values[i].index];

    for ( i = 0 ; i < nm ; i++ )
        (s->monitors)[i] = new_order[i];
}
#endif

struct rgb_triple *parse_color( const char *col )
{
    struct rgb_triple *rgb = malloc(sizeof(struct rgb_triple)); verify(rgb);

    if (col[0] == '#') {

        char *rr = malloc(3 * sizeof(char)); verify(rr);
        char *gg = malloc(3 * sizeof(char)); verify(gg);
        char *bb = malloc(3 * sizeof(char)); verify(bb);
        strncpy(rr, &(col[1]), 2); rr[2] = '\0';
        strncpy(gg, &(col[3]), 2); gg[2] = '\0';
        strncpy(bb, &(col[5]), 2); bb[2] = '\0';
        rgb->r = hextoint(rr);
        rgb->g = hextoint(gg);
        rgb->b = hextoint(bb);

        if (!(rgb->r >= 0 && rgb->r <= 255) ||
            !(rgb->g >= 0 && rgb->g <= 255) ||
            !(rgb->b >= 0 && rgb->b <= 255) ||
            strlen(col) != 7) {

            fprintf(stderr,
                    "Invalid hex code %s; defaulting to #000000.\n", col);
            rgb->r = rgb->g = rgb->b = 0;
        }
        free(rr); free(gg); free(bb);
    } else {
        XColor c;
        if (XParseColor(XDPY, COLORMAP, col, &c)) {
            rgb->r = c.red   / 257; // XParseColor returns 0 to 65535
            rgb->g = c.green / 257;
            rgb->b = c.blue  / 257;
        } else {
            fprintf(stderr, "Invalid color %s; defaulting to black.\n", col);
            rgb->r = rgb->g = rgb->b = 0;
        }
    }
    return rgb;
}

void
blank_screen( struct screen *s, const char *blank_color, Pixmap *canvas )
{
    struct rgb_triple *col = parse_color(blank_color);

    Imlib_Image bg = imlib_create_image(s->screen_width,
                                        s->screen_height);
    if (bg == NULL)
        die(1, "Failed to create image.");

    imlib_context_set_color(col->r, col->g, col->b, 255);
    imlib_context_set_drawable(*canvas);
    imlib_context_set_image(bg);
    imlib_image_fill_rectangle(0, 0, s->screen_width, s->screen_width);
    imlib_render_image_on_drawable(0, 0);
    imlib_free_image_and_decache();
    free(col);
}

void
solid_color( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;
    struct rgb_triple *col = wall->bgcol; // won't be null

    Imlib_Image solid_color = imlib_create_image(mon->width, mon->height);
    if (solid_color == NULL)
        die(1, "Failed to create image.");

    imlib_context_set_color(col->r, col->g, col->b, 255);
    imlib_context_set_image(solid_color);
    imlib_image_fill_rectangle(0, 0, mon->width, mon->height);
    wall->image = solid_color;
}

void
center_wall( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;
    struct rgb_triple *col = wall->bgcol;

    Imlib_Image centered_image = imlib_create_image(mon->width, mon->height);
    imlib_context_set_image(centered_image);
    imlib_context_set_color(0, 0, 0, 255);

    if (col != NULL)
        imlib_context_set_color(col->r, col->g, col->b, 255);

    imlib_image_fill_rectangle(0, 0, mon->width, mon->height);
    imlib_context_set_blend(1);

    /* this is where we place the image in absolute coordinates */
    /* this could lie outside the monitor, which is fine */
    int xtl = ((int) mon->width  - (int) wall->width)  / 2;
    int ytl = ((int) mon->height - (int) wall->height) / 2;

    imlib_blend_image_onto_image(wall->image, 0,
                                 0, 0, wall->width, wall->height,
                                 xtl + wall->xpos, ytl + wall->ypos,
                                 wall->width, wall->height);

    imlib_context_set_image(wall->image);
    imlib_free_image();
    imlib_context_set_image(centered_image);
    wall->image = centered_image;
    imlib_context_set_blend(0);
}

void
stretch_wall( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;
    struct rgb_triple *col = wall->bgcol;

    Imlib_Image stretched_image = imlib_create_image(mon->width, mon->height);
    imlib_context_set_image(stretched_image);
    imlib_context_set_color(0, 0, 0, 255);

    if (col != NULL)
        imlib_context_set_color(col->r, col->g, col->b, 255);

    imlib_image_fill_rectangle(0, 0, mon->width, mon->height);
    imlib_context_set_blend(1);

    imlib_blend_image_onto_image(wall->image, 0,
                                 0, 0, wall->width, wall->height,
                                 wall->xpos, wall->ypos,
                                 mon->width, mon->height);

    imlib_context_set_image(wall->image);
    imlib_free_image_and_decache();
    wall->image = stretched_image;
    imlib_context_set_blend(0);
}

void
fit_height( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;
    struct rgb_triple *col = wall->bgcol;

    float xg = wall->xg;

    Imlib_Image fit_height_image = imlib_create_image(mon->width, mon->height);
    imlib_context_set_image(fit_height_image);
    imlib_context_set_color(0, 0, 0, 255);

    if (col != NULL)
        imlib_context_set_color(col->r, col->g, col->b, 255);

    imlib_image_fill_rectangle(0, 0, mon->width, mon->height);
    imlib_context_set_blend(1);

    float scaled_width = wall->width * mon->height * (1.0 / wall->height);

    if (wall->xg_type == PERCENTAGE)
            xg = xg * scaled_width;

    /* trust me, the math is good. */
    float xtl = mon->width * 0.5 - xg;
    float max_xtl = abs(mon->width - scaled_width);

    if (scaled_width > mon->width) {
        if (xtl < -max_xtl)
            xtl = -max_xtl;
        else if (xtl > max_xtl)
            xtl = max_xtl;
    }

    imlib_blend_image_onto_image(wall->image, 0,
                                 0, 0, wall->width, wall->height,
                                 xtl + wall->xpos, 0 + wall->ypos,
                                 scaled_width, mon->height);

    imlib_context_set_image(wall->image);
    imlib_free_image_and_decache();
    wall->image = fit_height_image;
    imlib_context_set_blend(0);
}

void
fit_width( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;
    struct rgb_triple *col = wall->bgcol;

    float yg = wall->yg;

    Imlib_Image fit_width_image = imlib_create_image(mon->width, mon->height);
    imlib_context_set_image(fit_width_image);
    imlib_context_set_color(0, 0, 0, 255);

    if (col != NULL)
        imlib_context_set_color(col->r, col->g, col->b, 255);

    imlib_image_fill_rectangle(0, 0, mon->width, mon->height);
    imlib_context_set_blend(1);

    float scaled_height =  wall->height * mon->width * (1.0 / wall->width);

    if (wall->yg_type == PERCENTAGE)
            yg = yg * scaled_height;

    /* trust me, the math is good. */
    float ytl = mon->height * 0.5 - yg;
    float max_ytl = abs(mon->height - scaled_height);

    if (scaled_height > mon->height) {
        if (ytl < -max_ytl)
            ytl = -max_ytl;
        else if (ytl > max_ytl)
            ytl = max_ytl;
    }

    imlib_blend_image_onto_image(wall->image, 0,
                                 0, 0, wall->width, wall->height,
                                 0 + wall->xpos, ytl + wall->ypos,
                                 mon->width, scaled_height);

    imlib_context_set_image(wall->image);
    imlib_free_image_and_decache();
    wall->image = fit_width_image;
    imlib_context_set_blend(0);
}

void
fit_auto( struct monitor *mon )
{
    if (mon->width >= mon->height) { // for normal monitors
        if (   mon->wall->width * (1.0 / mon->wall->height)
            >= mon->width  * (1.0 / mon->height) )
            fit_width(mon);
        else
            fit_height(mon);
    } else { // for weird ass vertical monitors
        if (   mon->wall->height * (1.0 / mon->wall->width)
            >= mon->height * (1.0 / mon->width) )
            fit_height(mon);
        else
            fit_width(mon);
    }
}

void
zoom_fill( struct monitor *mon )
{
    if (mon->width >= mon->height) { // for normal monitors
        if (   mon->wall->width * (1.0 / mon->wall->height)
            >= mon->width  * (1.0 / mon->height) )
            fit_height(mon);
        else
            fit_width(mon);
    } else { // for weird ass vertical monitors
        if (   mon->wall->height * (1.0 / mon->wall->width)
            >= mon->height * (1.0 / mon->width) )
            fit_width(mon);
        else
            fit_height(mon);
    }
}

void
tile( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;

    Imlib_Image tiled_image = imlib_create_image(mon->width, mon->height);
    imlib_context_set_image(tiled_image);
    imlib_context_set_blend(1);

    /* tile image; the excess is cut off automatically by image size */
    for ( unsigned int yi = 0; yi <= mon->height; yi += (wall->height) )
        for ( unsigned int xi = 0; xi <= mon->width; xi += (wall->width) )
            imlib_blend_image_onto_image(wall->image, 0,
                                         0, 0,
                                         wall->width, wall->height,
                                         xi, yi,
                                         wall->width, wall->height);

    imlib_context_set_image(wall->image);
    imlib_free_image_and_decache();
    wall->image = tiled_image;
    imlib_context_set_blend(0);
}

void
make_greyscale( struct wallpaper *w )
{
    Imlib_Color color;
    imlib_context_set_image(w->image);
    unsigned int width = imlib_image_get_width();
    unsigned int height = imlib_image_get_height();
    float avg = 0.0;

    for( unsigned int x = 0; x < width; x++) {
        for( unsigned int y = 0; y < height; y++) {
            imlib_image_query_pixel(x, y, &color);
            avg = 0.21 * color.red + 0.72 * color.green + 0.07 * color.blue;
            imlib_context_set_color(avg, avg, avg, color.alpha);
            imlib_image_draw_pixel(x, y, 0);
        }
    }
}

void
tint_image( struct wallpaper *w )
{
    imlib_context_set_image(w->image);
    Imlib_Color_Modifier adjustments = imlib_create_color_modifier();
    verify(adjustments);

    imlib_context_set_color_modifier(adjustments);

    DATA8 r[256], g[256], b[256], a[256];
    imlib_get_color_modifier_tables (r, g, b, a);

    for (unsigned int i = 0; i < 256; i++) {
        r[i] = (DATA8) (((float) r[i] / 255.0) * (float) w->tint->r);
        g[i] = (DATA8) (((float) g[i] / 255.0) * (float) w->tint->g);
        b[i] = (DATA8) (((float) b[i] / 255.0) * (float) w->tint->b);
    }
    imlib_set_color_modifier_tables (r, g, b, a);
    imlib_apply_color_modifier();
    imlib_free_color_modifier();
}

void
flip_image( struct wallpaper *w )
{
    imlib_context_set_image(w->image);

    switch (w->axis) {
    case HORIZONTAL:
        imlib_image_flip_horizontal();
        break;
    case VERTICAL:
        imlib_image_flip_vertical();
        break;
    case DIAGONAL:
        imlib_image_flip_diagonal();
        break;
    default:
        break;
    } return;
}

void
change_contrast( struct wallpaper *w )
{
    imlib_context_set_image(w->image);
    Imlib_Color_Modifier adjustments = imlib_create_color_modifier();
    verify(adjustments);

    imlib_context_set_color_modifier(adjustments);
    imlib_modify_color_modifier_contrast(w->contrast);
    imlib_apply_color_modifier();
    imlib_free_color_modifier();
}

void
change_brightness( struct wallpaper *w )
{
    imlib_context_set_image(w->image);
    Imlib_Color_Modifier adjustments = imlib_create_color_modifier();
    verify(adjustments);

    imlib_context_set_color_modifier(adjustments);
    imlib_modify_color_modifier_contrast(w->brightness);
    imlib_apply_color_modifier();
    imlib_free_color_modifier();
}

void
blur_wall( struct wallpaper *w )
{
    imlib_context_set_image(w->image);
    imlib_image_blur(w->blur);
}

void
sharpen_wall( struct wallpaper *w )
{
    imlib_context_set_image(w->image);
    imlib_image_sharpen(w->sharpen);
}

void
parse_opts( unsigned int argc, char **args )
{
    /*VARIOUS FLAGS*/
    unsigned int store      = 0;
    unsigned int span       = 0;

    int assign_to_mon       = -1;

    char *bg_col            = NULL;
    char *tn_col            = NULL;

    unsigned int blur_r     = 0;
    unsigned int shrp_r     = 0;
    float contrast_v        = 1.0;
    float bright_v          = 0.0;

    unsigned int greyscale  = 0;

    fit_t     aspect        = FIT_AUTO;
    flip_t    axis          = NONE;

    gravity_t xg_type       = PERCENTAGE;
    gravity_t yg_type       = PERCENTAGE;

    float xg                  = 0.5;
    float yg                  = 0.5;

    char *image_path        = NULL;

    /* argument counter and argument buffer */
    unsigned int i;
    char *token;

    unsigned int max_mon_index = (SCREEN->num_mons) - 1;
    unsigned int cur_mon_index = 0;

    /*PARSE THE OPTIONS AND STORE IN APPROP. STRUCTS*/
    for (i = 1; i < argc; i++) {

        token = args[i];

        /*ignore geometry flags, these were already parsed in main*/
        if (streq(token, "--use-x-geometry") ||
            streq(token, "--use-y-geometry"))
            continue;

        /* if we have a flag and nothing after it */
        if (argc == i + 1 && token[0] == '-')
            tfargs_error(token);

        /*STORE FLAG MUST BE FIRST*/
        if            (streq(token, "--store") && i == 1) {
            store = 1;

        /*GLOBAL OPTIONS*/
        } else if    (streq(token, "--blank-color")) {
            BLANK_COLOR = args[++i];

        /*IMAGE FLAGS*/
        } else if    (streq(token, "--span")) {
            span = 1;
        } else if    (streq(token, "--bg-color")) {
            bg_col = args[++i];

        } else if    (streq(token, "--on")) {
#ifndef HAVE_LIBXINERAMA
            fprintf(stderr, "'setroot' was not compiled with Xinerama "
                            "support. '--on' is not supported. "
                            "Exiting with status 1.\n");
            exit(1);
#else
            assign_to_mon = parse_int(args[++i]);

            if (assign_to_mon < 0) {
                fprintf(stderr, "Monitor %d does not exist. "
                                "Exiting with status 1.\n",
                                assign_to_mon);
                exit(1);
            }
#endif
        /* MANIPULATIONS */
        } else if   (streq(token, "--greyscale")) {
            greyscale = 1;

        } else if   (streq(token, "--tint")) {
            tn_col = args[++i];

        } else if   (streq(token, "--blur")) {
            blur_r = parse_int(args[++i]);

        } else if   (streq(token, "--sharpen")) {
            shrp_r = parse_int(args[++i]);

        } else if   (streq(token, "--brighten")) {
            bright_v = parse_float(args[++i]);

        } else if   (streq(token, "--contrast")) {
            contrast_v = parse_float(args[++i]);

        } else if   (streq(token, "--fliph")) {
            axis = HORIZONTAL;
        } else if   (streq(token, "--flipv")) {
            axis = VERTICAL;
        } else if   (streq(token, "--flipd")) {
            axis = DIAGONAL;

        /* IMAGE OPTIONS */
        } else if   (streq(token, "-sc") || streq(token, "--solid-color" )) {
            bg_col = args[++i];
            image_path = "__COLOR__";

        } else if   (streq(token, "-c")  || streq(token, "--center")) {
            aspect = CENTER;
        } else if   (streq(token, "-s")  || streq(token, "--stretch")) {
            aspect = STRETCH;
        } else if   (streq(token, "-fh") || streq(token, "--fit-height")) {
            aspect = FIT_HEIGHT;
        } else if   (streq(token, "-fw") || streq(token, "--fit-width")) {
            aspect = FIT_WIDTH;
        } else if   (streq(token, "-gx") || streq(token, "--gravity-x")) {
            int last = strlen(args[++i]) - 1;
            if (args[i][last] == '%') {
                args[i][last] = '\0';
                xg_type = PERCENTAGE;
                xg = (float)parse_float(args[i])/100;
            } else {
                xg_type = POSITION;
                xg = parse_int(args[i]);
            }
        } else if   (streq(token, "-gy") || streq(token, "--gravity-y")) {
            int last = strlen(args[++i]) - 1;
            if (args[i][last] == '%') {
                args[i][last] = '\0';
                yg_type = PERCENTAGE;
                yg = parse_float(args[i])/100;
            } else {
                yg_type = POSITION;
                yg = (float)parse_int(args[i]);
            }
        } else if   (streq(token, "-f")  || streq(token, "--fit-auto")) {
            aspect = FIT_AUTO;
        } else if   (streq(token, "-z")  || streq(token, "--zoom")) {
            aspect = ZOOM;
        } else if   (streq(token, "-t")  || streq(token, "--tiled")) {
            aspect = TILE;

        /* if all of the above fail, treat it like an image path */
        } else {
            image_path = token;
        }
        /*assume anything that is not a flag ends one invocation sequence*/
        if (image_path == NULL) continue;

        /*if invocation blocks exceed number of monitors*/
        if (cur_mon_index > max_mon_index) {
            cur_mon_index = max_mon_index;
            goto RESET_FLAGS;
        }
        /*if no monitor is explicitly specified*/
        if (assign_to_mon == -1)
            assign_to_mon = cur_mon_index;

        /*if assigning to non-existent monitor*/
        if (assign_to_mon > (int) max_mon_index)
            goto RESET_FLAGS;

        cur_mon_index++;

        /* start creating the wall and assigning it to monitor */
        struct wallpaper *w = init_wall();

        if (!streq(image_path, "__COLOR__")) {
            Imlib_Image image = imlib_load_image(image_path);
            imlib_context_set_image(image);

            w->image  = image;
            w->width  = imlib_image_get_width();
            w->height = imlib_image_get_height();

            if (image == NULL) invalid_img_error(image_path);
        }
        w->image_path = image_path;
        w->option     = aspect;
        w->axis       = axis;
        w->span       = span;
        w->monitor    = assign_to_mon;
        w->xg_type    = xg_type;
        w->yg_type    = yg_type;

        if (greyscale != 0)     w->greyscale  = greyscale;
        if (blur_r != 0)        w->blur       = blur_r;
        if (shrp_r != 0)        w->sharpen    = shrp_r;

        if (bright_v != 0.0)    w->brightness = bright_v;
        if (contrast_v != 0.0)  w->contrast   = contrast_v;

        if (bg_col != NULL)     w->bgcol      = parse_color(bg_col);
        if (tn_col != NULL)     w->tint       = parse_color(tn_col);

        if (xg != 0.5)          w->xg         = xg;
        if (yg != 0.5)          w->yg         = yg;

        /*if for some reason a wall was already assigned to*/
        /*designated monitor, clean it*/
        if ((SCREEN->monitors)[w->monitor]->wall != NULL) {
            int taken_index = w->monitor;

            clean_wall((SCREEN->monitors)[taken_index]->wall);
            free((SCREEN->monitors)[taken_index]->wall);
            (SCREEN->monitors)[taken_index]->wall = NULL;

        } (SCREEN->monitors)[w->monitor]->wall = w;


RESET_FLAGS:
        span = greyscale = blur_r = shrp_r = 0;
        contrast_v = (bright_v = 0.0) + 1.0;
        axis = NONE;
        aspect = FIT_AUTO;
        tn_col = bg_col = NULL;
        image_path = NULL;
        assign_to_mon = -1;
        xg_type = PERCENTAGE;
        yg_type = PERCENTAGE;
        xg = 0.5;
        yg = 0.5;

    } // end for loop
    if (store) store_call(argc, args);
}

Pixmap
make_bg( struct screen *s )
{
    imlib_context_set_display(XDPY);
    imlib_context_set_visual(VISUAL);
    imlib_context_set_colormap(COLORMAP);

    imlib_set_cache_size(IMLIB_CACHE_SIZE);
    imlib_set_color_usage(IMLIB_COLOR_USAGE);
    imlib_context_set_dither(DITHER_SWITCH);

    Pixmap canvas = XCreatePixmap(XDPY,
                                  ROOT_WIN,
                                  s->screen_width,
                                  s->screen_height,
                                  BITDEPTH);

    imlib_context_set_drawable(canvas);
    blank_screen(s, BLANK_COLOR, &canvas);

    unsigned int i;
    unsigned int nm = s->num_mons;

    for (i = 0; i < nm; i++) {
        struct monitor *cur_mon = (s->monitors)[i];
        struct wallpaper *w = cur_mon->wall;

        if (w == NULL) continue;

        if (streq(w->image_path, "__COLOR__")) {
            solid_color(cur_mon);
        } else {
            /* ADJUSTMENTS TO IMAGE */
            if (w->axis == DIAGONAL) {
                int temp  = w->height;
                w->height = w->width;
                w->width  = temp;
            } flip_image(w);

            if (w->greyscale == 1)      make_greyscale(w);
            if (w->brightness != 0.0)   change_brightness(w);
            if (w->contrast != 1.0)     change_contrast(w);
            if (w->tint != NULL)        tint_image(w);

            /*if span, set dimensions of monitor to be screen*/
            if (w->span != 0) {
                cur_mon->width  = s->screen_width;
                cur_mon->height = s->screen_height;
                cur_mon->xpos   = 0;
                cur_mon->ypos   = 0;
            }
            switch (w->option) {
            case CENTER:
                center_wall(cur_mon);
                break;
            case STRETCH:
                stretch_wall(cur_mon);
                break;
            case FIT_HEIGHT:
                fit_height(cur_mon);
                break;
            case FIT_WIDTH:
                fit_width(cur_mon);
                break;
            case FIT_AUTO:
                fit_auto(cur_mon);
                break;
            case ZOOM:
                zoom_fill(cur_mon);
                break;
            case TILE:
                tile(cur_mon);
                break;
            default:
                break;
            }
            /*post process transformed image*/
            if (w->blur != 0)       blur_wall(w);
            if (w->sharpen != 0)    sharpen_wall(w);
        }
        /*set context and render image to drawable*/
        imlib_context_set_image(w->image);
        imlib_render_image_on_drawable(cur_mon->xpos + w->xpos,
                                       cur_mon->ypos + w->ypos);
        imlib_free_image_and_decache();
        w->image = NULL;
    }
    imlib_flush_loaders();
    return canvas;
}

int main(int argc, char** args)
{
    XDPY = XOpenDisplay(NULL);
    if (!XDPY) {
        fprintf(stderr, "%s: unable to open display '%s'\n",
                program_name, XDisplayName(NULL));
        exit(1);
    }
    if (argc < 2) {
        printf("No options were provided. Call \'man setroot\' for help.\n");
        exit(1);
    }

    XSCRN_NUM    = DefaultScreen(XDPY);
    XSCRN        = ScreenOfDisplay(XDPY, XSCRN_NUM);
    ROOT_WIN     = RootWindow(XDPY, XSCRN_NUM);
    COLORMAP     = DefaultColormap(XDPY, XSCRN_NUM);
    VISUAL       = DefaultVisual(XDPY, XSCRN_NUM);
    BITDEPTH     = DefaultDepth(XDPY, XSCRN_NUM);
    SCREEN       = init_screen(XSCRN->width, XSCRN->height);

#ifdef HAVE_LIBXINERAMA
    if          (streq(args[argc - 1], "--use-x-geometry"))
        sort_mons_by(SCREEN, SORT_BY_XORG);
    else if     (streq(args[argc - 1], "--use-y-geometry"))
        sort_mons_by(SCREEN, SORT_BY_YORG);
    else
        sort_mons_by(SCREEN, SORT_BY_XINM);
#endif

    if (argc > 1 && streq(args[1], "--restore")) {
        restore();
        goto CLEANUP;
    } Window desktop_window = None;

    parse_opts(argc, args);
    Pixmap bg = make_bg(SCREEN);

    if (bg != None) {
        set_pixmap_property(bg);
        XSetWindowBackgroundPixmap(XDPY, ROOT_WIN, bg);

        desktop_window = find_desktop(ROOT_WIN);

        if (desktop_window != None) {
            XSetWindowBackgroundPixmap(XDPY, desktop_window, bg);
            XClearWindow(XDPY, desktop_window);
        }
    }
    CLEANUP:

    clean_screen(SCREEN); free(SCREEN);

    XClearWindow(XDPY, ROOT_WIN);
    XFlush(XDPY);
    XCloseDisplay(XDPY);

    return EXIT_SUCCESS;
}
