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

	unsigned int		height;
	unsigned int		width;

	int					xpos;
	int					ypos;

	unsigned int		red;
	unsigned int		green;
	unsigned int		blue;

	unsigned int		blur;
	unsigned int		sharpen;

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

