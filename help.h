void show_ver()
{
    printf("setroot - v0.9 11-11-2014\n"\
           "(C) 2014 Tim Zhou\n");
}

void show_help()
{
    printf( "\n"\
            "NAME:\n"\
            "          setroot - sets your wallpaper.\n"\
            "\n"\
            "INVOCATION:\n"\
            "\n"\
            "          setroot [<storage flag>] [<global flags>] ( [<image flags>] [<image options>] <filename> )+ \n"\
            "\n"\
            "          The order of the filenames determine which monitor the wallpaper is set to.\n"\
            "          The first invoked filename is set to the first Xinerama monitor; the second\n"\
            "          to the second, and so on.\n"\
            "\n"\
            "          If more than one image option is supplied, the last one takes effect.\n"\
			"\n"\
            "          If n filenames are supplied for k monitors, where n > k, only the first\n"\
            "          k filenames are processed.\n"\
            "\n"\
            "STORAGE FLAG:\n"\
            "\n"\
            "    --store:\n"\
            "          stores valid invocation sequence in ~/.setroot-restore \n"\
            "\n"\
            "    --restore:\n"\
            "          looks for ~/.setroot-restore and calls invocation sequence \n"\
            "          (restores your previously set wallpapers and options)\n"\
            "\n"\
            "GLOBAL FLAGS:\n"\
            "\n"\
            "    --blank-color #RRGGBB:\n"\
			"		   if number of monitors exceeds number of walls, sets background \n"\
			"          color of blank walls, unless overriden by --bg-color\n"\
            "\n"\
            "IMAGE FLAGS:\n"\
            "\n"\
            "    --span:\n"\
            "          have image span all screens (no cropping) \n"\
            "          if more than one image is specified, the later image will be spanned.\n\n"\
            "          Note that this overrides the '--on' option. Note also that further\n"\
			"          images that are set (in the case of multiple monitors) will 'cover'\n"\
			"          the spanned image.\n"\
            "\n"\
            "    --bg-color #RRGGBB:\n"\
            "          set empty space around image to color\n"\
            "\n"\
			"    --on <n>\n"\
			"          assign image to be wallpaper on Xinerama monitor *n*.\n\n"\
			"          If not all images are passed this option, the unassigned walls\n"
		    "          will be placed on monitors by their position in the invocation sequence.\n\n"\
			"          Images which do have a specified assignment will then be assigned to their\n"\
			"          monitors, replacing any images which may have already been assigned.\n"\
			"\n"\
            "IMAGE MANIPULATIONS:\n"\
            "\n"\
            "    --blur <radius>:\n"\
            "          blur image\n"\
            "\n"\
            "    --sharpen <radius>:\n"\
            "          sharpen image\n"\
            "\n"\
            "    --brighten <amount>:\n"\
            "          modify image brightness by percentage (between -1 and 1)\n"\
            "\n"\
            "    --contrast <radius>:\n"\
            "          modify image contrast by percentage (between -100 and 100)\n"\
            "\n"\
            "    --flip[hvd]:\n"\
            "          flip image [h]orizontally, [v]ertically, or [d]iagonally\n"\
            "\n"\
            "    --tint #RRGGBB:\n"\
            "          tint image with color #RRGGBB\n"\
            "\n"\
            "IMAGE OPTIONS\n"\
            "\n"\
            "    -c,   --center:\n"\
            "          place unscaled image centered and cropped to screen\n"\
            "\n"\
            "    -t,   --tiled:\n"\
            "          tile image on invoked screen (Xinerama aware) \n"\
            "\n"\
            "    -s,   --stretch:\n"\
            "          stretch image (disregard aspect) on invoked screen \n"\
            "\n"\
            "    -z,   --zoom:\n"\
            "          scale image (preserve aspect) to fit screen completely (could cut off image) \n"\
            "\n"\
            "    -f,   --fit:\n"\
            "          scale image (preserve aspect) to fit screen (entire image on screen) - default setting \n"\
            "\n"\
            "    -fh,  --fit-height:\n"\
            "          scale image (preserve aspect) until height matches invoked screen \n"\
            "\n"\
            "    -fw,  --fit-width:\n"\
            "          scale image (preserve aspect) until width matches invoked screen \n"\
            "\n"\
            "    -sc, --solid-color #RRGGBB:\n"\
            "          set background to solid color #RRGGBB (hex code)\n"\
            "\n"\
            "MONITOR FLAGS\n"\
            "\n"\
            "    --use-x-geometry\n"\
            "          number Xinerama monitors from leftmost to rightmost\n"\
            "\n"\
            "    --use-y-geometry\n"\
            "          number Xinerama monitors from topmost to bottommost\n"\
            "\n"\
            );
}
