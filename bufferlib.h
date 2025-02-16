int utmp_open( char *filename );
int utmp_close();
int utmp_len();
struct utmp* utmp_getrec(int index);
void utmp_stats(int a[2]);