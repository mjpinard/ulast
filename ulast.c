#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <utmp.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "bufferlib.h"
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
void read_wtmp_file(char *info, char *username, int e_flag);
void print_session_info(struct utmp *login_record, struct utmp *logout_record);

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
    return NULL;
}

//update all log out times in the logout list on a reboot
void reboot_logs(struct Node** head, struct utmp *record){

    Node* current = *head;
    while(current!= NULL){
        current->data.ut_time = record->ut_time;
        current = current->next;
    }
}

//delete entire list of records
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
    char *username;
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
        fprintf(stderr,"You must enter a username to the program with [-f filepath] [e] username\n");
        exit(EXIT_FAILURE);
    }

    read_wtmp_file(info, username, e_flag);

    return 0;
}

void process_record(struct utmp* currRecord, char *username, Node** head){
    if(currRecord->ut_type == USER_PROCESS){ /* record is log in */
        if(strncmp(currRecord->ut_name, username,UT_NAMESIZE)==0){ /* check username */
            Node *matching_node = find_and_remove_matching_record(head, currRecord);
            if(matching_node != NULL){
                print_session_info(currRecord, &matching_node->data);
                free(matching_node);
            }else{
                print_session_info(currRecord, NULL);
            }
        }else{
            /* record could be implied log in */
            add_or_update_dead_process(head,currRecord); 
        }
    }else if(currRecord->ut_type == DEAD_PROCESS){
        add_or_update_dead_process(head,currRecord);
    }else if(currRecord->ut_type == BOOT_TIME){
        //set all the log out records to the current time
        reboot_logs(head,currRecord);
    }
}

void read_wtmp_file(char *info, char *username, int e_flag){
    int utmpfd;        /* read from this descriptor */
    struct utmp* currRecord;
    Node* head = NULL;

    if ((utmpfd = utmp_open(info)) == -1) /* open file */
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    int num_records = utmp_len();

    for(int i = num_records -1; i>=0; i--) /* read last record to first */
    {
        currRecord = utmp_getrec(i); /* get record */
        process_record(currRecord,username,&head); /* process each record */
    }
    freeLinkedList(head); /* clean up de */
    if(e_flag==1){
        int a[] = {0,0};
        utmp_stats(a);
        fprintf(stdout, "%d records read, %d buffer misses\n",a[0],a[1]);
    }
    utmp_close(utmpfd);
}


void print_session_info(struct utmp *login_record, struct utmp *logout_record) {
    time_t login_time = login_record->ut_time;
    time_t logout_time = logout_record ? logout_record->ut_time : time(NULL);
    double duration = difftime(logout_time, login_time);

    int hours = (int)(duration / 3600);
    int minutes = (int)((duration - (hours * 3600)) / 60);
    int seconds = (int)(duration - (hours * 3600) - (minutes * 60));

    char login_time_str[32];
    char logout_time_str[32];
    strftime(login_time_str, sizeof(login_time_str), "%a %b %d %H:%M", localtime(&login_time));
    if (logout_record) {
        strftime(logout_time_str, sizeof(logout_time_str), "%H:%M", localtime(&logout_time));
        printf("%-8.8s %-8.8s %-16.16s %s - %s (%02d:%02d)\n",
               login_record->ut_user, login_record->ut_line, login_record->ut_host, login_time_str, logout_time_str, hours, minutes);
    } else {
        printf("%-8.8s %-8.8s %-16.16s %s - still logged in\n",
               login_record->ut_user, login_record->ut_line, login_record->ut_host, login_time_str);
    }
}

void print_earliest_log(struct utmp *login_record){
    //print the earliest user record in the file
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
