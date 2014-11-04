typedef enum _FIT_TYPE {
    CENTER,
    TILE,
    STRETCH,
    FIT_AUTO,
    FIT_HEIGHT,
    FIT_WIDTH,
    ZOOM,
	COLOR
} fit_type;

struct wallpaper {
	Imlib_Image			image;

	unsigned int		height;
	unsigned int		width;

	unsigned int		red;
	unsigned int		green;
	unsigned int		blue;

	float				xpos;
	float				ypos;

	fit_type			option;
};

struct monitor {
    unsigned int		height;
	unsigned int		width;
    int					xpos;
	int					ypos;

	struct wallpaper	*wall;
};

