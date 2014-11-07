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

struct wallpaper {
	Imlib_Image			image;

	unsigned int		height, width;
	int					xpos, ypos;

	unsigned int		red, green, blue;

	unsigned int		blur;
	unsigned int		sharpen;

	fit_type			option;
	flip_type			axis;
};

struct monitor {
    unsigned int		height, width;
    int					xpos, ypos;

	struct wallpaper	*wall;
};

