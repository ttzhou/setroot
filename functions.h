void set_pixmap_property( Pixmap p );
Window find_desktop( Window window );

void show_ver();
void show_help();

void store_wall( int argc, char** line );
void restore_wall();

struct wallpaper *init_wall();
struct monitor *init_monitor( unsigned int w, unsigned int h,
							  unsigned int xp, unsigned int yp );

struct screen *init_screen( unsigned int sw, unsigned int sh );

struct rgb_triple *parse_color( const char *col );

void clean_wall( struct wallpaper *w );
void clean_monitor( struct monitor *m );
void clean_screen( struct screen *s );

#ifdef HAVE_LIBXINERAMA
void sort_mons_by( struct screen *s, int sort_opt );
#endif

void blank_screen( struct screen *s, const char *blank_color, Pixmap *canvas );

void center_wall( struct monitor *mon );
void stretch_wall( struct monitor *mon );
void fit_height( struct monitor *mon );
void fit_width( struct monitor *mon );
void fit_auto( struct monitor *mon );
void zoom_fill( struct monitor *mon );
void tile_wall( struct monitor *mon );
void solid_color( struct monitor *mon );

void make_greyscale( struct wallpaper *w );
void tint_image( struct wallpaper *w );
void flip_image( struct wallpaper *w );

void change_contrast( struct wallpaper * w);
void change_brightness( struct wallpaper * w);

void blur_wall( struct wallpaper * w);
void sharpen_wall( struct wallpaper * w);

void parse_opts( unsigned int argc, char **args );

Pixmap make_bg( struct screen *s );
