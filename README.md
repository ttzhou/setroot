NAME
----

**setroot** - sets your wallpaper.


INSTALLATION
------------

`sudo make install (clean)`


INVOCATION
----------

**setroot** [\<*storage flag*\>] { [\<*image flags*\>] [\<*image options*\>] \<filename\> }+

The order of the filenames determines which monitor the wallpaper is set to. The first invoked filename is set to the first Xinerama monitor; the second to the second; and so on.


STORAGE FLAGS
-------------

**--store**
> stores valid invocation sequence in `$HOME/.setroot-restore`

**--restore**
> looks for `$HOME/setroot-restore` and calls invocation sequence <br/> (restores your previously set wallpapers and options)


IMAGE FLAGS
-----------

**--span**
> have image span all screens (no cropping) <br/> if more than one image is specified, the later image will be spanned.

**--bg-color** *#RRGGBB*
> set empty space around image to color


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


AUTHOR
------

**(C) 2014** Tim Zhou \<ttzhou@uwaterloo.ca\>


ACKNOWLEDGMENTS
---------------

`set_pixmap_property()` is **(C) 1998** Michael Jennings \<mej@eterm.org\>

`find_desktop()` is a modification of  
`get_desktop_window()` **(C) 2004-2012** Jonathan Koren \<jonathan@jonathankoren.com\>
