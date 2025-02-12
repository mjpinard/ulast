#include	<stdio.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<utmp.h>
#include	<fcntl.h>
#include	<time.h>
#include	<stdlib.h>
#include "dumputmp.h"
/*
 *	read from utmp file the login records
 */

/* UTMP_FILE is a symbol defined in utmp.h */
/* note: compile with -DTEXTDATE for dates format: Feb  5, 1978 	*/
/*       otherwise, program defaults to NUMDATE (1978-02-05)		*/

#define TEXTDATE
#ifndef	DATE_FMT
   #   ifdef	TEXTDATE
#     define	DATE_FMT	"%b %e %H:%M"		/* text format	*/
#   else
#     define	DATE_FMT	"%Y-%m-%d %H:%M"	/* the default	*/
#   endif
#endif

void	show_info(struct utmp *);

int main(int ac, char *av[])
{
	struct utmp	utbuf;		/* read info into here */
	int		utmpfd;		/* read from this descriptor */
	char		*info = ( ac > 1 ? av[1] : WTMP_FILE); //hardcode at first
	ssize_t bytes_read;
	
    if ( (utmpfd = open( info, O_RDONLY )) == -1 ){
		perror(info);
		exit(1);
	}

    //Move file pointer to the end of the file
	
	if(lseek(utmpfd,0,SEEK_END)==-1){
		perror("lseek");
		close(utmpfd);
		exit(EXIT_FAILURE);
	}

	//Read the file backwards one struct at a time - start at begining of last struct
	while((bytes_read = lseek(utmpfd, -sizeof(utbuf),SEEK_CUR))!=-1){
		
		//get the first record and check that was successfully read into the buffer
		if(read(utmpfd,&utbuf,sizeof(utbuf))!=sizeof(utbuf)){
			perror("read");
			close(utmpfd);
			exit(EXIT_FAILURE);
		}

		//display login info
		show_utrec( &utbuf );

		//move file pointer back
		if(lseek(utmpfd,-sizeof(utbuf),SEEK_CUR)==-1){
			perror("lseek");
			close(utmpfd);
			exit(EXIT_FAILURE);
		}
	}

	close( utmpfd );
	return 0;	
}

/*
 *	show info()
 *			displays the contents of the utmp struct
 *			in human readable form
 *			* displays nothing if record has no user name
 */
void show_info( struct utmp *utbufp )
{
	void showtime(time_t, char *);

	if ( utbufp->ut_type != USER_PROCESS )
		return;

	printf("%-8.32s", utbufp->ut_name);		/* the logname	*/
		/* better: "%-8.*s",UT_NAMESIZE,utbufp->ut_name) */
	printf(" ");					/* a space	*/
 	printf("%-12.12s", utbufp->ut_id);		/* the tty	*/
	printf(" ");					/* a space	*/
	showtime( utbufp->ut_time , DATE_FMT);		/* display time	*/
	printf(" (%.256s)", utbufp->ut_host);	/*    show host	*/
	printf("\n");					/* newline	*/
}

#define	MAXDATELEN	100

void showtime( time_t timeval , char *fmt )
/*
 * displays time in a format fit for human consumption.
 * Uses localtime to convert the timeval into a struct of elements
 * (see localtime(3)) and uses strftime to format the data
 */
{
	char	result[MAXDATELEN];

	struct tm *tp = localtime(&timeval);		/* convert time	*/
	strftime(result, MAXDATELEN, fmt, tp);		/* format it	*/
	fputs(result, stdout);
}