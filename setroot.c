/********************************************************************************
 *
 *  Copyright (c) 2014 Tim Zhou <ttzhou@uwaterloo.ca>
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
 ********************************************************************************/

#define _POSIX_C_SOURCE 200809L // for getline

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>

#include <Imlib2.h>

#include "classes.h"
#include "functions.h"
#include "help.h"
#include "util.h"

/* globals */
Display            *XDPY;
unsigned int        DSCRN_NUM;
Screen             *DSCRN;
Window              ROOT_WIN;
int                 BITDEPTH;
Colormap            COLORMAP;
Visual             *VISUAL;

struct wallpaper   *WALLS = NULL;
struct monitor     *MONS  = NULL;
struct monitor      VSCRN; // spanned area of all monitors

unsigned int        NUM_MONS  = 0;

/* runtime variables */
static       char *program_name = "setroot";
static const char *BLANK_COLOR  = "black";

static const int   IMLIB_CACHE_SIZE  = 10240*1024; // I like big walls
static const int   IMLIB_COLOR_USAGE = 256;
static const int   DITHER_SWITCH     = 0;

static const int   SORT_BY_XINM = 0;
static const int   SORT_BY_XORG = 1;
static const int   SORT_BY_YORG = 2;

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
 *  way for self-gain. (Although I did change "ESETROOT_PMAP_ID" to
 *  "_SETROOT_PMAP_ID" for shameless self advertisement.)
 *
 *  For explanation, see http://www.eterm.org/docs/view.php?doc=ref and read
 *  section titled "Transparency".
 *
 *  This is used if you use a compositor that looks for _XROOTPMAP_ID.
 *
 *****************************************************************************/

void set_pixmap_property(Pixmap p)
{
    Atom prop_root, prop_setroot, type;
    int format;
    unsigned long length, after;
    unsigned char *data_root, *data_setroot;

    prop_root = XInternAtom(XDPY, "_XROOTPMAP_ID", True);
    prop_setroot = XInternAtom(XDPY, "_SETROOTPMAP_ID", True);

    if ((prop_root != None) && (prop_setroot != None)) {
        XGetWindowProperty(XDPY, ROOT_WIN, prop_root, 0L, 1L, False,
                AnyPropertyType, &type, &format,
                &length, &after, &data_root);

        if (type == XA_PIXMAP) {
            XGetWindowProperty(XDPY, ROOT_WIN, prop_setroot, 0L, 1L,
                    False, AnyPropertyType, &type, &format,
                    &length, &after, &data_setroot);

            if (data_root && data_setroot)
                if ((type == XA_PIXMAP) &&
                        (*((Pixmap *) data_root) == *((Pixmap *) data_setroot)))
                    XKillClient(XDPY, *((Pixmap *) data_root));

            if (data_setroot) { free(data_setroot); }
        }
        if (data_root) { free(data_root); }
    }

    prop_root = XInternAtom(XDPY, "_XROOTPMAP_ID", False);
    prop_setroot = XInternAtom(XDPY, "_SETROOTPMAP_ID", False);

    if (prop_root == None || prop_setroot == None)
        die(1, "creation of pixmap property failed");

    XChangeProperty(XDPY, ROOT_WIN, prop_root, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char *) &p, 1);
    XChangeProperty(XDPY, ROOT_WIN, prop_setroot, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char *) &p, 1);

    XSetCloseDownMode(XDPY, RetainPermanent);
    XFlush(XDPY);
}

/*****************************************************************************
 *
 *  find_desktop() is a slight modification of: get_desktop_window()
 *  which is (c) 2004-2012 Jonathan Koren <jonathan@jonathankoren.com>
 *
 *  find_desktop() finds the window that draws the desktop. This is mainly
 *  for those weird DEs that don't draw backgrounds to root window.
 *
 *  The original code was taken from imlibsetroot, by Jonathan Koren.
 *  His email is <jonathan@jonathankoren.com>.
 *
 *  I added the ability to search through all windows, not just children of root
 *  as his did. There's no performance penalty since 99.9% of the time
 *  it is just the root window and search ends immediately.
 *
 *  We call this on the root window to start, in main.
 *
 *****************************************************************************/

Window find_desktop( Window window )
{
    Atom prop_desktop, type;

    int format;
    unsigned long length, after;
    unsigned char *data;

    Window root, prnt;
    Window *chldrn;
    unsigned int n_chldrn = 0;

    // check if a desktop has been set by WM
    prop_desktop = XInternAtom(XDPY, "_NET_WM_DESKTOP", True);

    if (prop_desktop != None) {
        // start by checking the window itself
        if (XGetWindowProperty(XDPY, window, prop_desktop,
                    0L, 1L, False, AnyPropertyType,
                    &type, &format, &length, &after,
                    &data) == Success)
            return window;
        // otherwise run XQueryTree; if it fails, kill program
        if (!XQueryTree(XDPY, window, &root, &prnt, &chldrn, &n_chldrn)) {
            fprintf(stderr, "XQueryTree() failed!\n");
            exit(1);
        }
        // XQueryTree succeeded, but no property found on parent window,
        // so recurse on each child. Since prop_desktop exists, we
        // will eventually find it.
        for (unsigned int i = n_chldrn; i != 0; i--) { // makes for loop faster
            Window child = find_desktop(chldrn[i - 1]);
            if ( child != None &&
                    (XGetWindowProperty(XDPY, child, prop_desktop, 0L, 1L,
										False, AnyPropertyType, &type, &format,
										&length, &after, &data) == Success) )
                return child;
        }
        if (n_chldrn)
            XFree(chldrn); // pun not intended
        return None; // if we did not find a child with property on this node

    } else { // no _NET_WM_DESKTOP set, we just draw to root window
		fprintf(stderr,\
				"_NET_WM_DESKTOP not set; will draw to root window.\n");
        return ROOT_WIN;
    }
}

void store_wall( int argc, char** line )
{
    char *fn = strcat(getenv("HOME"),"/.setroot-restore");
    FILE *f = fopen(fn, "w");
    if (!f) {
        fprintf(stderr, "Could not write to file %s.\n", fn);
        exit(1);
    }
    int i;
    for (i = 2; i < argc - 1; i++) { // jump past setroot --store
        fprintf(f, line[i]);
        fprintf(f, " ");
    }
    fprintf(f, line[i]); // so we don't add space after last word
    fclose(f);
}

void restore_wall()
{
    FILE *f = fopen(strcat(getenv("HOME"),"/.setroot-restore"), "r");
    if (!f) {
        die(1, "Could not find file $HOME/.setroot-restore.\n");
    }
    size_t n = 0;
    char *line = NULL;
    if (getline(&line, &n, f) == -1) { // because fuck portability, I'm lazy
        fclose(f);
        die(1, "File corrupt, or file is empty.\n");
    }
    /* rarely get more than 10 arguments */
    char **args = malloc(10 * sizeof(char*)); verify(args);
    args[0] = program_name;

    /* break arg string into arg array and get argcount */
    unsigned int argc = 1;
    char *token = NULL;
    token = strtok(line, " ");

    while ( token != NULL ) {
        argc++;
        if (argc > 10)
            args = realloc(args, argc * sizeof(char*)); verify(args);
        args[argc - 1] = token;
        token = strtok(NULL, " ");
    }
    /* shrink to appropriate size, no need to verify */
	if (argc < 10)
		args = realloc(args, argc * sizeof(char*));

    parse_opts((int) argc, args);
    free(line);
    free(args);
    fclose(f);
}

void init_wall( struct wallpaper *w )
{
    w->height = w->width = 0;
    w->xpos   = w->ypos  = 0; // relative to monitor!

	w->span = w->monitor = 0;

    w->option = FIT_AUTO;
    w->axis   = NONE;

    w->blur       = w->sharpen  = 0;
    w->brightness = 0;
	w->contrast   = 1.0;
    w->grey       = 0;

    w->bgcol  = NULL;
    w->tint   = NULL;
    w->image  = NULL;
}

void clean_wall( struct wallpaper *w )
{
    if (w->bgcol != NULL)
        free(w->bgcol);
    if (w->tint != NULL)
        free(w->tint);
    if (w->image != NULL) {
        imlib_context_set_image(w->image);
        imlib_free_image_and_decache();
    }
}

void sort_mons_by( int sort_opt )
{
    if (!XineramaIsActive(XDPY)) {
        NUM_MONS = 1;
        MONS[0] = VSCRN;
        return;
    }
    XineramaScreenInfo *XSI
        = XineramaQueryScreens(XDPY, (int*) &NUM_MONS);
    MONS = malloc(sizeof(struct monitor) * NUM_MONS); verify(MONS);

    unsigned int i;
    struct pair values[NUM_MONS];

    if (sort_opt == SORT_BY_XORG) {
        for ( i = 0; i < NUM_MONS; i++ ) {
            values[i].value = XSI[i].x_org;
            values[i].index = i;
        }
        qsort(values, NUM_MONS, sizeof(struct pair), ascending);

    } else if (sort_opt == SORT_BY_YORG) {
        for ( i = 0; i < NUM_MONS; i++ ) {
            values[i].value = XSI[i].y_org;
            values[i].index = i;
        }
        qsort(values, NUM_MONS, sizeof(struct pair), ascending);

    } else {
        for ( i = 0; i < NUM_MONS; i++ )
            values[i].index = i;
    }
    for ( i = 0; i < NUM_MONS; i++ ) {
        MONS[i].height = XSI[values[i].index].height;
        MONS[i].width  = XSI[values[i].index].width;
        MONS[i].xpos   = XSI[values[i].index].x_org;
        MONS[i].ypos   = XSI[values[i].index].y_org;
        MONS[i].wall   = NULL;
    }
    XFree(XSI);
    XSync(XDPY, False);
}

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

            fprintf(stderr, "Invalid hex code %s; defaulting to #000000.\n", col);
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

void parse_opts( unsigned int argc, char **args )
{
    unsigned int rmbr      = 0;
    unsigned int span      = 0;
	unsigned int grey      = 0;
    int monitor            = -1;

    unsigned int blur_r    = 0;
    unsigned int sharpen_r = 0;
    float contrast_v       = 1.0;
    float bright_v         = 0;

    struct rgb_triple *bg_col   = NULL;
    struct rgb_triple *tint_col = NULL;

    fit_type flag  = FIT_AUTO;
    flip_type flip = NONE;

    /* set up monitors based on arrangement option */
    if        (streq(args[argc - 1], "--use-x-geometry")) {
        sort_mons_by(SORT_BY_XORG);
    } else if (streq(args[argc - 1], "--use-y-geometry")) {
        sort_mons_by(SORT_BY_YORG);
    } else {
        sort_mons_by(SORT_BY_XINM);
    }
    /* init array for storing wallpapers */
    WALLS = malloc(NUM_MONS * sizeof(struct wallpaper)); verify(WALLS);
	unsigned int num_walls = 0;

    for ( unsigned int i = 1 ; i < argc; i++ ) {
        /* STORAGE OPTIONS */
        if (streq(args[i], "--store") && i == 1) {
            rmbr = 1;

        /* GLOBAL OPTIONS */
        } else if (streq(args[i], "--blank-color")) {
            if (argc == i + 1) {
                fprintf(stderr, "Not enough arguments for %s.\n", args[i]);
                rmbr = 0;
                continue;
            }
            BLANK_COLOR = args[++i];

        /* IMAGE FLAGS */
        } else if (streq(args[i], "--span")) {
            span = 1;
        } else if (streq(args[i], "--bg-color")) {
            if (argc == i + 1) {
                fprintf(stderr, "Not enough arguments for %s.\n", args[i]);
                rmbr = 0;
                continue;
            }
            bg_col = parse_color(args[++i]);

        } else if (streq(args[i], "--on")) {
            if (argc == i + 1) {
                fprintf(stderr, "Not enough arguments for %s.\n", args[i]);
                rmbr = 0;
                continue;
            }
            if (!isdigit(args[i + 1][0])) {
                fprintf(stderr, \
                        "No Xinerama monitor %s. Ignoring '--on' option. \n",\
                        args[++i]);
                rmbr = 0;
                continue;
            }
            monitor = atoi(args[++i]);

            if (monitor > (int) (NUM_MONS - 1) || monitor < 0) {
                fprintf(stderr, \
                        "No Xinerama monitor %d. Ignoring '--on' option. \n",\
                        monitor);
                monitor = -1;
                rmbr = 0;
                continue;
            }

        /* MANIPULATIONS */
        } else if (streq(args[i], "--greyscale")) {
			grey = 1;

        } else if (streq(args[i], "--tint")) {
            if (argc == i + 1) {
                fprintf(stderr, "No color specified.\n");
                rmbr = 0;
                continue;
            }
            tint_col = parse_color(args[++i]);

        } else if (streq(args[i], "--blur")) {
            if (argc == i + 1) {
                fprintf(stderr, "Blur radius not specified.\n");
                rmbr = 0;
                continue;
            }
            if (!(isdigit(args[i + 1][0]))) {
                fprintf(stderr, \
                        "Invalid blur amount %s. No blur applied.\n", \
                        args[++i]);
                rmbr = 0;
                continue;
            }
            blur_r = atoi(args[++i]);

        } else if (streq(args[i], "--sharpen")) {
            if (argc == i + 1) {
                fprintf(stderr, "Sharpen radius not specified.\n");
                rmbr = 0;
                continue;
            }
            if (!(isdigit(args[i + 1][0]))) {
                fprintf(stderr, \
                        "Invalid sharpen amount %s. No sharpen applied.\n",\
                        args[++i]);
                rmbr = 0;
                continue;
            }
            sharpen_r = atoi(args[++i]);

        } else if (streq(args[i], "--brighten")) {
            if (argc == i + 1) {
                fprintf(stderr, "Brightness amount not specified.\n");
                rmbr = 0;
                continue;
            }
            if (!isdigit(args[i + 1][0]) && args[i + 1][0] != '-') {
                fprintf(stderr, \
                        "Invalid brightness %s. No brightening applied.\n",\
                        args[++i]);
                rmbr = 0;
                continue;
            }
            bright_v = strtof(args[++i], NULL);

        } else if (streq(args[i], "--contrast")) {
            if (argc == i + 1) {
                fprintf(stderr, "Contrast amount not specified.\n");
                rmbr = 0;
                continue;
            }
            if (!isdigit(args[i + 1][0]) && args[i + 1][0] != '-') {
                fprintf(stderr, \
                        "Invalid contrast %s. No contrast applied.\n",\
                        args[++i]);
                rmbr = 0;
                continue;
            }
            contrast_v = strtof(args[++i], NULL);

        } else if (streq(args[i], "--fliph")) {
            flip = HORIZONTAL;
        } else if (streq(args[i], "--flipv")) {
            flip = VERTICAL;
        } else if (streq(args[i], "--flipd")) {
            flip = DIAGONAL;

        /* IMAGE OPTIONS */
        } else if (streq(args[i], "-sc") || streq(args[i], "--solid-color" )) {
            if (argc == i + 1) {
                fprintf(stderr, "Not enough arguments for %s.\n", args[i]);
                rmbr = 0;
                continue;
            }
            bg_col = parse_color(args[i + 1]);
            flag = COLOR;

        } else if (streq(args[i], "-c")  || streq(args[i], "--center")) {
            flag = CENTER;
        } else if (streq(args[i], "-s")  || streq(args[i], "--stretch")) {
            flag = STRETCH;
        } else if (streq(args[i], "-fh") || streq(args[i], "--fit-height")) {
            flag = FIT_HEIGHT;
        } else if (streq(args[i], "-fw") || streq(args[i], "--fit-width")) {
            flag = FIT_WIDTH;
        } else if (streq(args[i], "-f")  || streq(args[i], "--fit-auto")) {
            flag = FIT_AUTO;
        } else if (streq(args[i], "-z")  || streq(args[i], "--zoom")) {
            flag = ZOOM;
        } else if (streq(args[i], "-t")  || streq(args[i], "--tiled")) {
            flag = TILE;

        /* GET IMAGE AND STORE OPTIONS */
        } else {
            num_walls++;
            init_wall(&(WALLS[num_walls - 1]));

            if (flag != COLOR && // won't try to load image if flag is COLOR
                    !(WALLS[num_walls - 1].image = imlib_load_image(args[i]))) {
                fprintf(stderr, "Image %s not found.\n", args[i]);
                exit(1);
            }
            WALLS[num_walls - 1].option = flag;
			flag = FIT_AUTO; // reset flag

            if (monitor > -1) {
                WALLS[num_walls - 1].monitor = monitor;
                monitor = -1;
            } else {
                WALLS[num_walls - 1].monitor = num_walls - 1;
            }
            if (bg_col != NULL) {
                WALLS[num_walls - 1].bgcol = bg_col;
                bg_col = NULL;
            } else {
                WALLS[num_walls - 1].bgcol = parse_color("black");
            }
            if (flip != NONE) {
                WALLS[num_walls - 1].axis = flip;
                flip = NONE;
            }
			if (grey) {
				WALLS[num_walls - 1].grey = grey--;
			}
            if (blur_r) {
                WALLS[num_walls - 1].blur = blur_r;
                blur_r = 0;
            }
            if (sharpen_r) {
                WALLS[num_walls - 1].sharpen = sharpen_r;
                sharpen_r = 0;
            }
            if (bright_v) {
                WALLS[num_walls - 1].brightness = bright_v;
                bright_v = 0;
            }
            if (contrast_v != 1.0) {
                WALLS[num_walls - 1].contrast = contrast_v;
                contrast_v = 1.0;
            }
            if (tint_col != NULL) {
                WALLS[num_walls - 1].tint = tint_col;
                tint_col = NULL;
            }
			if (span) {
				WALLS[num_walls - 1].span = 1;
				span = 0;
			}
            if (num_walls == NUM_MONS) // at most one wall per screen or span
                break;
        }
    }
    if (!num_walls) {
        fprintf(stderr, "Warning: no images were supplied.\n");
        rmbr = 0;
    }
    if (rmbr)
        store_wall(argc, args);

    /* shrink WALLS array appropriately */
    if (num_walls < NUM_MONS) {
		/* don't need to verify since it is shrinking prechecked memory */
        WALLS = realloc(WALLS, num_walls * sizeof(struct wallpaper));
    }
    /* assign walls to monitors */
    for (unsigned int wn = 0; wn < num_walls; wn++) {
        int mn = WALLS[wn].monitor;
        /* if wall was previously assigned to mon, clear it */
        if (MONS[mn].wall != NULL)
            clean_wall(MONS[mn].wall);
        MONS[mn].wall = &(WALLS[wn]);
    }
}

void clear_background( const char *blank_color )
{
    struct rgb_triple *col = parse_color(blank_color);

    Imlib_Image bg = imlib_create_image(VSCRN.width, VSCRN.height);
    if (bg == NULL)
        die(1, "Failed to create image.");

    imlib_context_set_color(col->r, col->g, col->b, 255);
    imlib_context_set_image(bg);
    imlib_image_fill_rectangle(0, 0, VSCRN.width, VSCRN.height);
	imlib_render_image_on_drawable(0, 0);
	imlib_free_image_and_decache();
    free(col);
}

void solid_color( struct monitor *mon )
{
    struct wallpaper *fill = mon->wall;
    struct rgb_triple *col = fill->bgcol;
    fill->bgcol = NULL;

    Imlib_Image solid_color = imlib_create_image(mon->width, mon->height);
    if (solid_color == NULL)
        die(1, "Failed to create image.");

    imlib_context_set_color(col->r, col->g, col->b, 255);
    imlib_context_set_image(solid_color);
    imlib_image_fill_rectangle(0, 0, mon->width, mon->height);
    free(col);
}

void center_wall( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;

    Imlib_Image centered_image = imlib_context_get_image();
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
    imlib_context_set_blend(0);
}

void stretch_wall( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;

    Imlib_Image stretched_image = imlib_context_get_image();
    imlib_context_set_blend(1);

    imlib_blend_image_onto_image(wall->image, 0,
                                 0, 0, wall->width, wall->height,
                                 wall->xpos, wall->ypos,
                                 mon->width, mon->height);

    imlib_context_set_image(wall->image);
    imlib_free_image();
    imlib_context_set_image(stretched_image);
    imlib_context_set_blend(0);
}

void fit_height( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;

    Imlib_Image fit_height_image = imlib_context_get_image();
    imlib_context_set_blend(1);

    float scaled_width = wall->width * mon->height * (1.0 / wall->height);
    /* trust me, the math is good. */
    float xtl = (mon->width - scaled_width) * 0.5;

    imlib_blend_image_onto_image(wall->image, 0,
                                 0, 0, wall->width, wall->height,
                                 xtl + wall->xpos, 0 + wall->ypos,
                                 scaled_width, mon->height);

    imlib_context_set_image(wall->image);
    imlib_free_image();
    imlib_context_set_image(fit_height_image);
    imlib_context_set_blend(0);
}

void fit_width( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;

    Imlib_Image fit_width_image = imlib_context_get_image();
    imlib_context_set_blend(1);

    float scaled_height =  wall->height * mon->width * (1.0 / wall->width);
    /* trust me, the math is good. */
    float ytl = (mon->height - scaled_height) * 0.5;

    imlib_blend_image_onto_image(wall->image, 0,
                                 0, 0, wall->width, wall->height,
                                 0 + wall->xpos, ytl + wall->ypos,
                                 mon->width, scaled_height);

    imlib_context_set_image(wall->image);
    imlib_free_image();
    imlib_context_set_image(fit_width_image);
    imlib_context_set_blend(0);
}

void fit_auto( struct monitor *mon )
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

void zoom_fill( struct monitor *mon ) // basically works opposite of fit_auto
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

void tile( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;

    Imlib_Image tiled_image = imlib_context_get_image();
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
    imlib_free_image();
    imlib_context_set_image(tiled_image);
    imlib_context_set_blend(0);
}

void make_greyscale( Imlib_Image *image )
{
	Imlib_Color color;
	imlib_context_set_image(image);
	unsigned int width = imlib_image_get_width();
	unsigned int height = imlib_image_get_height();

	for( unsigned int x = 0; x < width; x++) {
		for( unsigned int y = 0; y < height; y++) {
			imlib_image_query_pixel(x, y, &color);
			float avg = 0.21 * color.red + 0.72 * color.green + 0.07 * color.blue;
			imlib_context_set_color(avg, avg, avg, color.alpha);
			imlib_image_draw_pixel(x, y, 0);
		}
	}
}

void tint_wall( struct monitor *mon )
{
    struct wallpaper *wall = mon->wall;

    DATA8 r[256], g[256], b[256], a[256];
    imlib_get_color_modifier_tables (r, g, b, a);

    struct rgb_triple *tint = wall->tint;
    wall->tint = NULL;

    for (unsigned int i = 0; i < 256; i++) {
        r[i] = (DATA8) (((float) r[i] / 255.0) * (float) tint->r);
        g[i] = (DATA8) (((float) g[i] / 255.0) * (float) tint->g);
        b[i] = (DATA8) (((float) b[i] / 255.0) * (float) tint->b);
    }
    imlib_set_color_modifier_tables (r, g, b, a);
    free(tint);
}

Pixmap make_bg()
{
    imlib_context_set_display(XDPY);
    imlib_context_set_visual(VISUAL);
    imlib_context_set_colormap(COLORMAP);

    imlib_set_cache_size(IMLIB_CACHE_SIZE);
    imlib_set_color_usage(IMLIB_COLOR_USAGE);
    imlib_context_set_dither(DITHER_SWITCH);

    Pixmap canvas
        = XCreatePixmap(XDPY,
                        ROOT_WIN,
                        DSCRN->width,
                        DSCRN->height,
                        BITDEPTH);

    imlib_context_set_drawable(canvas);
	clear_background(BLANK_COLOR);

    for (unsigned int i = 0; i < NUM_MONS; i++) {
        struct monitor *cur_mon = &(MONS[i]);
        struct wallpaper *cur_wall = cur_mon->wall;

		if (cur_wall == NULL)
			continue;

        fit_type option = cur_wall->option;

        if (cur_wall->span) {
			cur_mon = &(VSCRN);
			cur_mon->wall = cur_wall;
		}
		/* color the bg (also serves as canvas for img) */
		solid_color(cur_mon);

		if (option != COLOR) {
            /* the "canvas" for our image, colored by solid_color above */
            Imlib_Image bg = imlib_context_get_image();

            /* load image, set dims */
            imlib_context_set_image(cur_wall->image);
            cur_wall->width  = imlib_image_get_width();
            cur_wall->height = imlib_image_get_height();

            /* flip image */
            switch (cur_wall->axis) {
            case NONE:
                break;
            case HORIZONTAL:
                imlib_image_flip_horizontal();
                break;
            case VERTICAL:
                imlib_image_flip_vertical();
                break;
            case DIAGONAL:
                /* switch its dimensions */
                cur_wall->width = imlib_image_get_height();
                cur_wall->height = imlib_image_get_width();
                imlib_image_flip_diagonal();
                break;
            }
			/* greyscale */
			if (cur_wall->grey)
				make_greyscale(cur_wall->image);

            /* adjust image before we set background */
            Imlib_Color_Modifier adjustments = imlib_create_color_modifier();
            if (adjustments == NULL)
                die(1, MEMORY_ERROR);
            imlib_context_set_color_modifier(adjustments);

            if (cur_wall->tint != NULL)
                tint_wall(cur_mon);
            if (cur_wall->brightness)
                imlib_modify_color_modifier_brightness(cur_wall->brightness);
            if (cur_wall->contrast != 1.0)
                imlib_modify_color_modifier_contrast(cur_wall->contrast);

            imlib_apply_color_modifier();
            imlib_free_color_modifier();

            imlib_context_set_image(bg);

            /* size image */
            switch (option) {
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
            /* post process image */
            if (cur_wall->blur)
                imlib_image_blur(cur_wall->blur);
            if (cur_wall->sharpen)
                imlib_image_sharpen(cur_wall->sharpen);
        }
        /* render the bg */
        imlib_render_image_on_drawable_at_size(cur_mon->xpos + cur_wall->xpos,
                                               cur_mon->ypos + cur_wall->ypos,
                                               cur_mon->width,
                                               cur_mon->height);
        imlib_free_image_and_decache();
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
    DSCRN_NUM    = DefaultScreen(XDPY);
    DSCRN        = ScreenOfDisplay(XDPY, DSCRN_NUM);
    ROOT_WIN     = RootWindow(XDPY, DSCRN_NUM);
    COLORMAP     = DefaultColormap(XDPY, DSCRN_NUM);
    VISUAL       = DefaultVisual(XDPY, DSCRN_NUM);
    BITDEPTH     = DefaultDepth(XDPY, DSCRN_NUM);

    VSCRN.height = DSCRN->height;
    VSCRN.width  = DSCRN->width;
    VSCRN.wall   = NULL;
    VSCRN.xpos   = 0;
    VSCRN.ypos   = 0;

    /* check for acts of desperation */
	if (argc < 2 || streq(args[1], "-h") || streq(args[1], "--help")) {
        show_help();
        exit(EXIT_SUCCESS);
    }
    /* restore or parse options */
    if (argc > 1 && streq(args[1], "--restore"))
        restore_wall();
    else
        parse_opts(argc, args);

    Pixmap bg = make_bg();

    if (bg) {
        set_pixmap_property(bg);
		XSetWindowBackgroundPixmap(XDPY, find_desktop(ROOT_WIN), bg);
    }
    free(MONS);
    free(WALLS);
    XClearWindow(XDPY, ROOT_WIN);
    XFlush(XDPY);
    XCloseDisplay(XDPY);

    return EXIT_SUCCESS;
}
