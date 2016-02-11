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

void clean_wall( struct wallpaper *w );
void clean_monitor( struct monitor *m );
void clean_screen( struct screen *s );

#ifdef HAVE_LIBXINERAMA
void sort_mons_by( struct station *s, int sort_opt );
#endif

void parse_opts( unsigned int argc, char **args );

void center_wall( struct monitor *mon );
void stretch_wall( struct monitor *mon );
void fit_height( struct monitor *mon );
void fit_width( struct monitor *mon );
void fit_auto( struct monitor *mon );
void zoom_fill( struct monitor *mon );
void tile_wall( struct monitor *mon );
void solid_color( struct monitor *mon );

void tint_wall( struct monitor *mon );
void brighten( struct monitor *mon );
void contrast( struct monitor *mon );

Pixmap make_bg();

struct rgb_triple *parse_color( const char *col );
