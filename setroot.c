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

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>

#include <Imlib2.h>

#include "util.h"
#include "classes.h"
#include "functions.h"
#include "help.h"

/* globals */
Display            *XDPY;
unsigned int        DEFAULT_SCREEN_NUM;
Screen             *DEFAULT_SCREEN;
Window              ROOT_WIN;
int                 BITDEPTH;
Colormap            COLORMAP;
Visual             *VISUAL;

struct wallpaper   *WALLS;
struct monitor     *MONS;
struct monitor      VSCRN; // spanned area of all monitors

unsigned int       NUM_MONS;
unsigned int       SPAN_WALL;

static char *program_name   = "setroot";

/* runtime variables */
const int IMLIB_CACHE_SIZE  = 10240*1024; // I like big walls
const int IMLIB_COLOR_USAGE = 256;
const int DITHER_SWITCH     = 0;

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

            free(data_setroot); // should free the ID string as well
        }
        free(data_root); // should free the ID string as well
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
        fprintf(stderr, "_NET_WM_DESKTOP not set; will draw to root window.\n");
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
    /* shrink to appropriate size */
    args = realloc(args, argc * sizeof(char*)); verify(args);
    parse_opts((int) argc, args);
    free(line);
    free(args);
    fclose(f);
}

void init_wall( struct wallpaper *w )
{
    w->height = w->width   = 0;
    w->xpos   = w->ypos    = 0; // relative to monitor!

    w->option = FIT_AUTO;
    w->axis   = NONE;

    w->blur       = w->sharpen  = 0;
    w->brightness = w->contrast = 0;

    w->bgcol  = NULL;
    w->tint   = NULL;
    w->image  = NULL;
}

struct rgb_triple *parse_color( char *col )
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

            printf("Invalid hex code %s; defaulting to #000000.\n", col);
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
            printf("Invalid color %s; defaulting to black.\n", col);
            rgb->r = rgb->g = rgb->b = 0;
        }
    }
    return rgb;
}

void parse_opts( unsigned int argc, char **args )
{
    if (argc < 2) {
        show_help();
        exit(EXIT_SUCCESS);
    }
    unsigned int nwalls    = 0;
    unsigned int rmbr      = 0;
    char* blank_color      = "black";

    unsigned int blur_r    = 0;
    unsigned int sharpen_r = 0;
    float contrast_v       = 0;
    float bright_v         = 0;

    struct rgb_triple *bg_col   = NULL;
    struct rgb_triple *tint_col = NULL;

    fit_type flag  = FIT_AUTO;
    flip_type flip = NONE;

    /* init array for storing wallpapers */
    WALLS = malloc(NUM_MONS * sizeof(struct wallpaper)); verify(WALLS);

    for ( unsigned int i = 1 ; i < argc; i++ ) {
        if (streq(args[i], "-h")  || streq(args[i], "--help")) {
            show_help();
            exit(EXIT_SUCCESS);

        /* STORAGE OPTIONS */
        } else if (streq(args[i], "--store") && i == 1) {
            rmbr = 1;

        /* GLOBAL OPTIONS */
        } else if (streq(args[i], "--blank-color")) {
            if (argc == i + 1) {
                fprintf(stderr, "No color specified.\n");
                continue;
            }
            blank_color = args[++i];

        /* IMAGE FLAGS */
        } else if (streq(args[i], "--span")) {
            SPAN_WALL = 1;
        } else if (streq(args[i], "--bg-color")) {
            if (argc == i + 1) {
                fprintf(stderr, "No color specified.\n");
                continue;
            }
            bg_col = parse_color(args[++i]);

        /* MANIPULATIONS */
        } else if (streq(args[i], "--tint")) {
            if (argc == i + 1) {
                fprintf(stderr, "No color specified.\n");
                continue;
            }
            tint_col = parse_color(args[++i]);

        } else if (streq(args[i], "--blur" )) {
            if (argc == i + 1) {
                fprintf(stderr, "Blur radius not specified.\n");
                continue;
            }
            if (!(isdigit(args[i + 1][0])))
                fprintf(stderr, \
                        "Invalid blur amount %s. No blur will be applied.\n", \
                        args[i + 1]);
            blur_r = atoi(args[++i]);

        } else if (streq(args[i], "--sharpen" )) {
            if (argc == i + 1) {
                fprintf(stderr, "Sharpen radius not specified.\n");
                continue;
            }
            if (!(isdigit(args[i + 1][0])))
                fprintf(stderr, \
                        "Invalid sharpen amount %s. No sharpen will be \
                        applied.\n", args[i + 1]);
            sharpen_r = atoi(args[++i]);

        } else if (streq(args[i], "--brighten" )) {
            if (argc == i + 1) {
                fprintf(stderr, "Brightness amount not specified.\n");
                continue;
            }
            bright_v = strtod(args[++i], NULL);

        } else if (streq(args[i], "--contrast" )) {
            if (argc == i + 1) {
                fprintf(stderr, "Contrast amount not specified.\n");
                exit(1);
            }
            contrast_v = strtod(args[++i], NULL);

        } else if (streq(args[i], "--fliph" )) {
            flip = HORIZONTAL;
        } else if (streq(args[i], "--flipv" )) {
            flip = VERTICAL;
        } else if (streq(args[i], "--flipd" )) {
            flip = DIAGONAL;

        /* IMAGE OPTIONS */
        } else if (streq(args[i], "-sc") || streq(args[i], "--solid-color" )) {
            if (argc == i + 1) {
                fprintf(stderr, "Not enough arguments.\n");
                continue;
            }
            bg_col = parse_color(args[i+1]);
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
            nwalls++;
            init_wall(&(WALLS[nwalls - 1]));

            WALLS[nwalls - 1].option = flag;

            if (flag != COLOR && // won't try to load image if flag is COLOR
                    !(WALLS[nwalls - 1].image = imlib_load_image(args[i]))) {
                fprintf(stderr, "Image %s not found.\n", args[i]);
                exit(1);
            }
            if (flip != NONE) {
                WALLS[nwalls - 1].axis = flip;
                flip = NONE;
            }
            if (blur_r) {
                WALLS[nwalls - 1].blur = blur_r;
                blur_r = 0;
            }
            if (sharpen_r) {
                WALLS[nwalls - 1].sharpen = sharpen_r;
                sharpen_r = 0;
            }
            if (bright_v) {
                WALLS[nwalls - 1].brightness = bright_v;
                bright_v = 0;
            }
            if (contrast_v) {
                WALLS[nwalls - 1].contrast = contrast_v;
                contrast_v = 0;
            }
            if (tint_col != NULL) {
                WALLS[nwalls - 1].tint = tint_col;
                tint_col = NULL;
            }
            if (bg_col != NULL) {
                WALLS[nwalls - 1].bgcol = bg_col;
                bg_col = NULL;
            } else {
                WALLS[nwalls - 1].bgcol = parse_color("black");
            }
            if (SPAN_WALL) {
                VSCRN.wall = &(WALLS[nwalls - 1]);
                break; // remove this line if you want to span the latest wall
            }
            if (nwalls == NUM_MONS) // at most one wall per screen or span
                break;
        }
    }
    if (!nwalls) {
        fprintf(stderr, "No images were supplied.\n");
        exit(0);
    }
    if (rmbr)
        store_wall(argc, args);

    /* assign walls to monitors */
    for (unsigned int mn = 0; mn < NUM_MONS; mn++) {
        if (mn >= nwalls) { // fill remaining monitors with blank walls
            init_wall(&(WALLS[mn]));
            WALLS[mn].option = COLOR;
            WALLS[mn].bgcol  = parse_color(blank_color);
        }
        MONS[mn].wall = &(WALLS[mn]);
    }
}

void color_bg( struct monitor *mon )
{
    struct wallpaper *fill = mon->wall;
    struct rgb_triple *col = fill->bgcol;
    fill->bgcol = NULL;

    Imlib_Image color_bg = imlib_create_image(mon->width, mon->height);
    if (color_bg == NULL)
        die(1, "Failed to create image.");

    imlib_context_set_color(col->r, col->g, col->b, 255);
    imlib_context_set_image(color_bg);
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
                        DEFAULT_SCREEN->width,
                        DEFAULT_SCREEN->height,
                        BITDEPTH);

    imlib_context_set_drawable(canvas);

    for (unsigned int i = 0; i < NUM_MONS; i++) {
        struct monitor *cur_mon;
        if (SPAN_WALL)
            cur_mon = &(VSCRN);
        else
            cur_mon = &(MONS[i]);

        color_bg(cur_mon);

        struct wallpaper *cur_wall = cur_mon->wall;
        fit_type option = cur_wall->option;

        if (option != COLOR) {
            /* the "canvas" for our image, colored by color_bg above */
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
            /* adjust image before we set background */
            Imlib_Color_Modifier adjustments = imlib_create_color_modifier();
            if (adjustments == NULL)
                die(1, MEMORY_ERROR);
            imlib_context_set_color_modifier(adjustments);

            if (cur_wall->tint != NULL)
                tint_wall(cur_mon);
            if (cur_wall->brightness)
                imlib_modify_color_modifier_brightness(cur_wall->brightness);
            if (cur_wall->contrast)
                imlib_modify_color_modifier_contrast(cur_wall->contrast);

            imlib_apply_color_modifier();
            imlib_free_color_modifier();

            imlib_context_set_image(bg);

            /* size image */
            switch (option) {
            case CENTER:
                center_wall(cur_mon);
                break;
            case STRETCH: // taken care of by render_on_drawable
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
            /* manipulate image */
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

        if (SPAN_WALL)
            break;
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
    DEFAULT_SCREEN_NUM = DefaultScreen(XDPY);
    DEFAULT_SCREEN     = ScreenOfDisplay(XDPY, DEFAULT_SCREEN_NUM);
    ROOT_WIN           = RootWindow(XDPY, DEFAULT_SCREEN_NUM);
    COLORMAP           = DefaultColormap(XDPY, DEFAULT_SCREEN_NUM);
    VISUAL             = DefaultVisual(XDPY, DEFAULT_SCREEN_NUM);
    BITDEPTH           = DefaultDepth(XDPY, DEFAULT_SCREEN_NUM);

    VSCRN.height = DEFAULT_SCREEN->height;
    VSCRN.width  = DEFAULT_SCREEN->width;
    VSCRN.wall   = NULL;
    VSCRN.xpos   = 0;
    VSCRN.ypos   = 0;

    if (XineramaIsActive(XDPY)) {
        XineramaScreenInfo *XSI
            = XineramaQueryScreens(XDPY, (int*) &NUM_MONS);
        MONS = malloc(sizeof(struct monitor) * NUM_MONS); verify(MONS);
        for (unsigned int i = 0; i < NUM_MONS; i++) {
            MONS[i].height = XSI[i].height;
            MONS[i].width  = XSI[i].width;
            MONS[i].xpos   = XSI[i].x_org;
            MONS[i].ypos   = XSI[i].y_org;
            MONS[i].wall   = NULL;
        }
        XFree(XSI);
        XSync(XDPY, False);
    } else {
        NUM_MONS = 1;
        MONS[0] = VSCRN;
    }
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
