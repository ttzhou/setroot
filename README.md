NAME
----

**setroot** - sets your wallpaper.

VERSION
-------

`pre-release: version 0.95 - 2014-11-11`


ABOUT
-----

This program was written as a way for me to learn some C, and also to serve as a
personal replacement to the excellent `imlibsetroot` and `feh` programs.

I liked `feh`'s syntax, but I had no use for its image viewing capabilities, so I
tried out `imlibsetroot`. I encountered some segfaults/Imlib errors during
usage, so I started hacking it, but ended up just writing my own program based
off of it, stripped of extraneous features I saw no need for.

This program in no way constitutes good code. I have absolutely no experience in
formal programming or software development, I am merely a lowly math major.

Please, if you have stumbled upon this repository somehow and actually use this
program, feel free to critique the hell out of my code and help me improve. My
ego died many ages ago in undergrad.


INSTALLATION
------------

`sudo make install (clean)`


DEPENDENCIES
------------

`imlib2`


INVOCATION
----------

**setroot** [\<*storage flag*\>] [\<*global flag*\>] { [\<*image flags*\>] [\<*image options*\>] \<filename\> }+ [\<monitor flag\>]

The order of the filenames determines which monitor the wallpaper is set to. The
first invoked filename is set to the first Xinerama monitor; the second to the
second; and so on.

If more than one image option is applied, the last one takes effect.

If *n* filenames are supplied for *k* monitors, where *n* > *k*, only the first
*k* filenames are processed.


STORAGE FLAGS
-------------

**--store**
> stores valid invocation sequence in `$HOME/.setroot-restore`

**--restore**
> looks for `$HOME/setroot-restore` and calls invocation sequence <br/> (restores your previously set wallpapers and options)

GLOBAL FLAGS
------------

**--blank-color** *#RRGGBB*
> if number of monitors exceeds number of walls, sets background <br/> color of blank walls, unless overriden by **--bg-color**


IMAGE FLAGS
-----------

**--span**
> <p>have image span all screens (no cropping) <br/> if more than one image is specified, the later image will be spanned.</p><p>Note that this overrides the `--on` option. Note also that further images that are set (in the case of multiple monitors) will "cover" the spanned image.</p>

**--bg-color** *#RRGGBB*
> set empty space around image to color

**--on** \<*n*\>
> assign image to be wallpaper on Xinerama monitor *n*. <p>if not all images are passed this option, the unassigned walls will be placed on monitors by their position in the invocation sequence. images which do have a specified assignment will then be assigned to their monitors, replacing any images which may have already been assigned.</p>


IMAGE MANIPULATIONS
-------------------

**--blur** \<*radius*\>
> blur image

**--sharpen** \<*radius*\>
> sharpen image

**--brighten** \<*amount*\>
> modify image brightness by amount (between -1 and 1)

**--contrast** \<*amount*\>
> modify contrast of image by amount (between -100 and 100)

**--flip**[*hvd*]
> flip image [*h*]orizontally, [*v*]ertically, or [*d*]iagonally

**--tint** *#RRGGBB*
> tint image with color *#RRGGBB*


IMAGE OPTIONS
-------------

**-h, --help**
> show this help

**-c, --center**
> place unscaled image centered and cropped to screen

**-t, --tiled**
> tile image on invoked screen (Xinerama aware)

**-s, --stretch**
> stretch image (disregard aspect) on invoked screen

**-z, --zoom**
> scale image (preserve aspect) to fit screen completely (could cut off image)

**-f, --fit**
> scale image (preserve aspect) to fit screen (entire image on screen) - default setting

**-fh, --fit-height**
> scale image (preserve aspect) until height matches invoked screen

**-fw, --fit-width**
> scale image (preserve aspect) until width matches invoked screen

**-sc, --solid-color #RRGGBB**
> set background to solid color #RRGGBB (hex code)


MONITOR FLAGS
-------------

**--use-x-geometry**
> number Xinerama monitors from leftmost to rightmost

**--use-y-geometry**
> number Xinerama monitors from topmost to bottommost


TODO
----

+ Write a man page

AUTHOR
------

**(C) 2014** Tim Zhou \<ttzhou@uwaterloo.ca\>


ACKNOWLEDGMENTS
---------------

`set_pixmap_property()` is **(C) 1998** Michael Jennings \<mej@eterm.org\>

`find_desktop()` is a modification of  
`get_desktop_window()` **(C) 2004-2012** Jonathan Koren \<jonathan@jonathankoren.com\>
