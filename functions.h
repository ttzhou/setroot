void set_pixmap_property(Pixmap p);
Window find_desktop();

void show_ver();
void show_help();

void store_wall( int argc, char** line );
void restore_wall();

void init_wall( struct wallpaper *w );

void parse_opts( unsigned int argc, char **args );
struct rgb_triple *parse_color( char *col );

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
