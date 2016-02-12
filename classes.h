typedef enum FIT_TYPE {
    CENTER,
    TILE,
    STRETCH,
    FIT_AUTO,
    FIT_HEIGHT,
    FIT_WIDTH,
    ZOOM,
	COLOR
} fit_t;

typedef enum FLIP_TYPE {
	NONE,
	HORIZONTAL,
	VERTICAL,
	DIAGONAL
} flip_t;

struct rgb_triple { int r, g, b; };

#ifdef HAVE_LIBXINERAMA
// used for quicksort
struct pair {
	unsigned int index;
	int value;
};

// comparison functions for pairs
int
ascending( const void *a, const void *b )
{
    return ( (*((struct pair*) a)).value - (*((struct pair*) b)).value);
}

int
descending( const void *a, const void *b )
{
    return ( (*((struct pair*) b)).value - (*((struct pair*) a)).value);
}
#endif

struct screen {
	struct monitor		**monitors;
	unsigned int		num_mons;
	unsigned int		screen_width;
	unsigned int		screen_height;
};

struct monitor {
    unsigned int		width;
    unsigned int		height;

    unsigned int		xpos;
    unsigned int		ypos;

	struct wallpaper	*wall;
};

struct wallpaper {
	Imlib_Image			image;
	char 				*image_path;

	unsigned int		span;
	unsigned int		monitor;

	unsigned int		width;
	unsigned int		height;

	int					xpos;
	int					ypos;

	struct rgb_triple	*bgcol;
	struct rgb_triple	*tint;

	unsigned int		greyscale;
	unsigned int		blur;
	unsigned int		sharpen;
	float				contrast;
	float				brightness;

	fit_t				option;
	flip_t				axis;
};

