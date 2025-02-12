#include	<time.h>
#include 	<stdlib.h>
#include	<stdio.h>

/* long2date.c -- translate from long ints to ctime form dates */
/*
 *  usage: long2date [longint ...]
 *   args: if long ints on command line, then convert those
 *         if no args, then read long ints from stdin until EOF
 *   outp: for each long int, print the long int, a tab, the ctime result
 *   rets: 0 for ok, 1 for unconvertable longint
 */

void process_args(int, char **);
void process_stdin();
void process_one( unsigned long );

int main(int ac, char **av)
{
	if ( ac > 1 )
		process_args( ac, av );
	else
		process_stdin();
	return 0;
}

void process_one( unsigned long ttt )
{
	char	*str;

	str = ctime( &ttt );
	if ( str == NULL ){
		fprintf(stderr, "bad int %lu\n", ttt);
		exit(1);
	}
	printf("%ld\t%s", ttt, str);
}

void process_args( int ac, char **av )
{
	int i;

	for ( i = 1 ; i < ac ; i++ )
		process_one( strtol( av[i], NULL, 10 ) );
}

void process_stdin()
{
	long int ttt;

	while( scanf("%ld", &ttt ) == 1 )
		process_one( ttt );
}
