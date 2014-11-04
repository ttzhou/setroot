void set_pixmap_property(Pixmap p);

Window find_desktop();

void show_help();

void store_wall( int argc, char** line );
void restore_wall();

void init_wall( struct wallpaper *w );

int* parse_color( char *col );
void parse_opts( unsigned int argc, char **args );

void color_background( struct monitor *mon );
void center_wall( struct monitor *mon );
void stretch_wall( struct monitor *mon );
void fit_height( struct monitor *mon );
void fit_width( struct monitor *mon );
void fit_auto( struct monitor *mon );
void zoom_fill( struct monitor *mon );
void tile( struct monitor *mon );

Pixmap make_bg();
