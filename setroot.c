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

Window find_desktop( Window window )
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

		XFree(children);
		return child;
	} return None;
}

void store_wall( int argc, char** args )
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

void restore_wall()
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
		exit(1);
	} free(fn);
}

void init_wall( struct wallpaper *w )
{
    w->height     = w->width    = 0;
    w->xpos       = w->ypos     = 0; // relative to monitor!
	w->span       = w->monitor  = 0;

    w->option     = FIT_AUTO;
    w->axis       = NONE;

    w->brightness = 0;
	w->contrast   = 1.0;

    w->blur       = w->sharpen  = 0;
    w->brightness = 0;
	w->contrast   = 1.0;
    w->grey       = 0;

    w->bgcol      = NULL;
    w->tint       = NULL;
    w->image      = NULL;
}

void
clean_station( struct station *s )
{
}

void
clean_wall( struct wallpaper *w )
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

#ifdef HAVE_LIBXINERAMA
void sort_mons_by( int sort_opt )
{
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

void parse_opts( unsigned int argc, char **args )
{
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

	Window desktop_window = None;

    /* check for acts of desperation */
	if (argc < 2 || streq(args[1], "-h") || streq(args[1], "--help")) {
        printf("No options provided. Call \'man setroot\' for help.\n");
        exit(EXIT_SUCCESS);
    }
    if (argc > 1 && streq(args[1], "--restore")) {
        restore_wall();
		goto CLEANUP;
	}
	parse_opts(argc, args);
    Pixmap bg = make_bg();

    if (bg) {
        set_pixmap_property(bg);
		XSetWindowBackgroundPixmap(XDPY, ROOT_WIN, bg);

		desktop_window = find_desktop(ROOT_WIN);

		if (desktop_window != None) {
			XSetWindowBackgroundPixmap(XDPY, desktop_window, bg);
			XClearWindow(XDPY, desktop_window);
		}
    }

	CLEANUP:

    free(MONS);
    free(WALLS);
    XClearWindow(XDPY, ROOT_WIN);
    XFlush(XDPY);
    XCloseDisplay(XDPY);

    return EXIT_SUCCESS;
}
