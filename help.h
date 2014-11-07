void show_ver()
{
    printf("setroot - v0.3 11-06-2014\n"\
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
            "          setroot [<storage flag>] ( [<image flags>] [<image options>] <filename> )+ \n"\
            "\n"\
            "          The order of the filenames determine which monitor the wallpaper is set to.\n"\
            "          The first invoked filename is set to the first Xinerama monitor; the second\n"\
            "          to the second, and so on.\n"\
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
            "IMAGE FLAGS:\n"\
            "\n"\
            "    --span:\n"\
            "          have image span all screens (no cropping) \n"\
            "          if more than one image is specified, the first invoked image will be spanned. \n"\
            "\n"\
            "    --bg-color #RRGGBB:\n"\
            "          set empty space around image to color\n"\
            "\n"\
            "IMAGE MANIPULATIONS:\n"\
            "\n"\
            "    --blur <radius>:\n"\
            "          blur image\n"\
            "\n"\
            "    --tint #RRGGBB:\n"\
            "          tint image with color\n"\
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
            );
}
