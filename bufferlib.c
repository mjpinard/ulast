/* bufferlib.c  - functions to buffer reads from utmp file that can be read at any index
 *
 *      functions are
 *              utmp_open(char *filename)    - open file
 *                      returns -1 on error
 *              struct utmp *utmp_getrec((int index))      - return pointer to struct at given index
 *                      returns NULL on eof or read error
 *              int utmp_close()               - close file
 *              utmp_stats(int a[2])           - reports buffering efficiency
 *              utmp_len()                     - returns the number of records in the file
 * 
 */

#include        <stdio.h>
#include        <fcntl.h>
#include        <sys/types.h>
#include        <utmp.h>
#include	    <unistd.h>
#include	    "bufferlib.h"

#define NRECS   2
#define UTSIZE  (sizeof(struct utmp))

static	struct utmp utmpbuf[NRECS];	/* storage		*/
static  int     num_recs;		/* num stored   	*/
static int buffer_start;                        /* keeps track of buffer start index   	*/
static int buffer_end;                          /* keeps track of buffer end index   	*/
static  int     fd_utmp = -1;           /* read from    	*/
static ssize_t file_size_bytes;
static int buffer_reloads;
static int records_read;
static int buffer_size = NRECS; // default buffer size



/*
 * utmp_open -- connect to specified file
 *  args: name of a utmp file
 *  rets: -1 if error, fd for file otherwise
 */
int utmp_open( char *filename )
{
        fd_utmp = open( filename, O_RDONLY );           /* open it      */
        buffer_start = buffer_end = -1;
        num_recs = buffer_reloads = records_read = 0;        /* initialize all values  */
        return fd_utmp;                                 /* report       */
}

/*
 * utmp_close -- disconnenect
 *  args: none
 *  rets: ret value from close(2)  -- see man 2 close
 */
int utmp_close()
{
	int rv = 0;
        if ( fd_utmp != -1 ){                   /* don't close if not   */
                rv = close( fd_utmp );          /* open                 */
		fd_utmp = -1;			/* record as closed	*/
	}
	return rv;
}

/*
 * utmp_len -- gets and returns # of records, sets the number of records and calculates the buffer size
 *  rets: -1 if error, int # records otherwise
 */
int utmp_len(){
    file_size_bytes = lseek(fd_utmp, 0, SEEK_END);
    if(file_size_bytes==-1){
        perror("lseek");
    }
    // move file pointer back
    if (lseek(fd_utmp, 0, SEEK_SET) == -1)
    {
        perror("lseek");
        return -1;
    }
    num_recs = file_size_bytes/UTSIZE;

    return num_recs;
}

struct utmp* utmp_getrec(int index){
    size_t bytes_read;
    //check if the index is in the buffer, if it is in the buffer, return the value
    if(index>= buffer_start && index <= buffer_end){
        return &utmpbuf[index-buffer_start];
    }
    //move to the start of the buffer
    if(lseek(fd_utmp, (index/ NRECS)* NRECS * UTSIZE, SEEK_SET) == -1){
        return NULL;
    }

    //read records into the buffer
    bytes_read = read(fd_utmp, utmpbuf, NRECS*UTSIZE);
    if(bytes_read == -1){
        return NULL;
    }
    buffer_reloads++; /* udpate buffer load count */
    
    buffer_start = (index/NRECS)* UTSIZE; /* update buffer start */
    buffer_end = buffer_start + ((bytes_read/UTSIZE)-1); /* update buffer end */

    //return pointer to record
    if(index >= buffer_start && index<= buffer_end){
        return &utmpbuf[index - buffer_start];
    }else{
        return NULL;
    }
}

//need to check that this will persist to calling method
void utmp_stats(int a[2]){
    a[0] = records_read;
    a[1] = buffer_reloads;
    return a;

}
