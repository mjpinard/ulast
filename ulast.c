#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <utmp.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
/*
 *	who version 2		- read /var/run/utmp and list info therein
 * 				- surpresses empty records
 *				- formats time nicely
 */

/* UTMP_FILE is a symbol defined in utmp.h */
/* note: compile with -DTEXTDATE for dates format: Feb  5, 1978 	*/
/*       otherwise, program defaults to NUMDATE (1978-02-05)		*/

#define TEXTDATE
#ifndef DATE_FMT
#ifdef TEXTDATE
#define DATE_FMT "%b %e %H:%M" /* text format	*/
#else
#define DATE_FMT "%Y-%m-%d %H:%M" /* the default	*/
#endif
#endif

void show_info(struct utmp *);
void read_wtmp_file(char *info, char *username);

// Create a node
typedef struct Node {
    struct utmp data;
    struct Node* next;
} Node;

  // Function to create a new Node
Node* createNode(struct utmp *data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Failed to allocate memory for new node");
        exit(1);
    }
    newNode->data = *data;
    newNode->next = NULL;
    return newNode;
}

void insertAtEnd(Node** head, struct utmp *data) {
    Node* newNode = createNode(data);
    if (*head == NULL) {
        *head = newNode;
        return;
    }
    Node* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newNode;
}

// add or update log out

void add_or_update_dead_process(struct Node** head, struct utmp *record){
    if(record->ut_type != DEAD_PROCESS){
        return;
    }
    
    Node* current = *head;
    while(current!= NULL){
        if(strncmp(current->data.ut_line, record->ut_line, UT_LINESIZE)==0){
            current->data.ut_pid = record->ut_pid;
            current->data.ut_time = record->ut_time;
            current->data.ut_session = record->ut_session;
            return;
        }
        current = current->next;
    }
    insertAtEnd(head, record);
}

//function to return the matching node and remove it from the linked list, returns a null node* if the matching log in record does not exist

Node* find_and_remove_matching_record(Node **head, struct utmp *record){
    Node *current = *head;
    Node *previous = NULL;
    
    while(current!= NULL){
        //if the record matches
        if(current->data.ut_pid == record->ut_pid && strncmp(current->data.ut_line, record->ut_line, UT_LINESIZE)==0){
            // if record is first in the list
            if(previous ==NULL){
                *head = current->next;
            }else{
                //remove from the list
                previous->next = current->next;
            }
            return current;
        }
        previous = current;
        current = current->next;
    }
}

  //update all log out times on reboot

  void freeLinkedList(Node* head) {
    Node* current = head;
    while (current != NULL) {
        Node* next = current->next;
        free(current);
        current = next;
    }
}


int main(int ac, char *av[])
{
    char *info = WTMP_FILE;
    char *username = "dce-lib215";
    int e_flag = 0;

    //process args
    for(int i = 1; i< ac; i++){
		if(strcmp(av[i], "-f")== 0 && i + 1 <ac){
			info = av[i+1];
			i++; //skip next arg bc it is the filepath
		}else if(strcmp(av[i], "-e")== 0){
			e_flag = 1;
		}else if(av[i][0] == '-'){
            fprintf(stderr, "Unknown option: %s\n", av[i]);
            exit(EXIT_FAILURE);
        }
        else{
			username = av[i];
		}
	}
    if(username ==NULL){
        fprintf(stderr,"Usage: %s [-f filepath] [e] username\n", av[0]);
        exit(EXIT_FAILURE);
    }
    printf("username is: %s. \n",username);
    printf("filename is: %s. \n",info);
    printf("eflag is: %d. \n", e_flag);


    read_wtmp_file(info, username);

    return 0;
}

void read_wtmp_file(char *info, char *username){
    struct utmp utbuf; /* read info into here */
    int utmpfd;        /* read from this descriptor */
    ssize_t bytes_read;
    Node* head = NULL;


    if ((utmpfd = open(info, O_RDONLY)) == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Move file pointer to the end of the file
    if (lseek(utmpfd, 0, SEEK_END) == -1)
    {
        perror("lseek");
        close(utmpfd);
        exit(EXIT_FAILURE);
    }
    // Read the file backwards one struct at a time - start at begining of last struct
    while ((bytes_read = lseek(utmpfd, -sizeof(utbuf), SEEK_CUR)) != -1)
    {

        // get the first record and check that was successfully read into the buffer
        if (read(utmpfd, &utbuf, sizeof(utbuf)) != sizeof(utbuf))
        {
            perror("read");
            close(utmpfd);
            exit(EXIT_FAILURE);
        }

        if(utbuf.ut_type == USER_PROCESS){
            if(strncmp(utbuf.ut_name, username,UT_NAMESIZE)!=0){
                Node *matching_node = find_and_remove_matching_record(&head, &utbuf);
                if(matching_node != NULL){
                    time_t login_time = utbuf.ut_time;
                    time_t logout_time = matching_node->data.ut_time;
                    double duration = difftime(logout_time, login_time);
                    
                    printf("User: %-8.8s Line: %-8.8s Login: %s Logout: %s Duration: %.2f seconds Host: %-16.16s\n", 
                    utbuf.ut_user, utbuf.ut_line, ctime(&login_time), ctime(&logout_time), duration, utbuf.ut_host);

                    free(matching_node);
                }

            }
        }
        add_or_update_dead_process(&head,&utbuf);


        // //if it's a log out, add it to the stack
        // if(utbuf.ut_type == DEAD_PROCESS){
        //     //update the log out time and pid if it exists, otherwise add to the stack
        //     add_or_update_dead_process(&head,&utbuf);
        // }

        // if(utbuf.ut_type == USER_PROCESS){
        //     //matches the user we are looking for
        //     if(strncmp(utbuf.ut_name, username,UT_NAMESIZE)!=0){
        //         //this is your correct user
        //         printf("this is your user)");
        //         printf("\n");
        //         //process the user, print the session time
        //         Node *matching_node = find_and_remove_matching_record(&head, &utbuf);
                // if(matching_node != NULL){
                //     time_t login_time = utbuf.ut_time;
                //     time_t logout_time = matching_node->data.ut_time;
                //     double duration = difftime(logout_time, login_time);
                    
                //     printf("User: %-8.8s Line: %-8.8s Login: %s Logout: %s Duration: %.2f seconds Host: %-16.16s\n", 
                //     utbuf.ut_user, utbuf.ut_line, ctime(&login_time), ctime(&logout_time), duration, utbuf.ut_host);

                //     free(matching_node);
                // }
        //     }
        //     }
        // }



        // move file pointer back
        if (lseek(utmpfd, -sizeof(utbuf), SEEK_CUR) == -1)
        {
            perror("lseek");
            close(utmpfd);
            exit(EXIT_FAILURE);
        }
    
    }

    Node* current = head;
    while(current->next!=NULL){
            show_info(&(current->data));
            current = current->next;
    }

    close(utmpfd);
}


/*
 *	show info()
 *			displays the contents of the utmp struct
 *			in human readable form
 *			* displays nothing if record has no user name
 */
void show_info(struct utmp *utbufp)
{
    void showtime(time_t, char *);

    // if (utbufp->ut_type != USER_PROCESS)
    //     return;

    printf("%-8.32s", utbufp->ut_name);       /* the logname	*/
                                              /* better: "%-8.*s",UT_NAMESIZE,utbufp->ut_name) */
    printf(" ");                              /* a space	*/
    printf("%-12.12s", utbufp->ut_line);      /* the tty	*/
    printf(" ");                              /* a space	*/
    printf("%6d ", utbufp->ut_pid ); 
    showtime(utbufp->ut_time, DATE_FMT);      /* display time	*/
    if (utbufp->ut_host[0] != '\0')           /* if not ""	*/
        printf(" (%.256s)", utbufp->ut_host); /*    show host	*/
    printf("\n");                             /* newline	*/
}

#define MAXDATELEN 100

void showtime(time_t timeval, char *fmt)
/*
 * displays time in a format fit for human consumption.
 * Uses localtime to convert the timeval into a struct of elements
 * (see localtime(3)) and uses strftime to format the data
 */
{
    char result[MAXDATELEN];

    struct tm *tp = localtime(&timeval);   /* convert time	*/
    strftime(result, MAXDATELEN, fmt, tp); /* format it	*/
    fputs(result, stdout);
}







//method to find the matching log out record

//method to add log out record to the list 

//

//if find username log in record
    // traverse for matching log out record


//reboot method
//set all records to have the most recent reboot time as logout time

