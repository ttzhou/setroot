typedef enum FIT_TYPE {
    CENTER,
    TILE,
    STRETCH,
    FIT_AUTO,
    FIT_HEIGHT,
    FIT_WIDTH,
    ZOOM,
	COLOR
} fit_type;

typedef enum FLIP_TYPE {
	NONE,
	HORIZONTAL,
	VERTICAL,
	DIAGONAL
} flip_type;

struct rgb_triple {
	int r, g, b;
};

struct pair {
	unsigned int index;
	int value;
};

struct wallpaper {
	Imlib_Image			image;
	char*				fullpath;

	unsigned int		span;
	int					monitor;

	unsigned int		height;
	unsigned int		width;

	int					xpos;
	int					ypos;

	struct rgb_triple	*bgcol;
	struct rgb_triple	*tint;

	unsigned int		grey;
	unsigned int		blur;
	unsigned int		sharpen;
	float				contrast;
	float				brightness;

	fit_type			option;
	flip_type			axis;
};

struct monitor {
    unsigned int		height;
    unsigned int		width;

    unsigned int		xpos;
    unsigned int		ypos;

	struct wallpaper	*wall;
};

// comparison functions for pairs
int ascending( const void *a, const void *b )
{
    return ( (*((struct pair*) a)).value - (*((struct pair*) b)).value);
}

int descending( const void *a, const void *b )
{
    return ( (*((struct pair*) b)).value - (*((struct pair*) a)).value);
}
