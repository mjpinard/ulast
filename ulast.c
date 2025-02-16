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
 *	ulast		- read /var/run/utmp or given file and returns log in records for a user
 *              - contains linked list struct in order to process the utmp records
 *              - main function is 
 */

 //global variables to keep track of system reboots and shutdowns
static time_t latest_reboot = 0;
static time_t latest_shutdown = 0;

void read_wtmp_file(char *info, char *username, int e_flag);
void print_session_info(struct utmp *login_record, struct utmp *logout_record);

// Data structure to define a single log out record
typedef struct Node {
    struct utmp data;
    struct Node* next;
} Node;


//Helper functions for log out data structure

/*
 * createNode -- creates a new Node instance which holds a utmp struct and next pointer
 *  args: pointer to utmp struct
 *  rets: pointer to new node
 */

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

/*
 * insertAtEnd -- adds a new log out record to the end of a linked list
 *  args: pointer to utmp struct, head pointer
 */

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

/*
 *  add_or_update_dead_process -- checks whether the record is already in the linked list
 *  and ether updates with the log out and pid or adds the new record to the list
 *  args: pointer to utmp struct, head pointer
 */

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

/*
 *  find_and_remove_matching_record -- checks whether the log out record is in the linked list
 *  and returns that record if it exists, otherwise returns the null pointer
 *  args: pointer to utmp struct, head pointer
 *  rets: corresponding log out record, null ptr if it doesn't exist
 */
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


/*
 *  reboot_logs -- updates all the log records to the new reboot time
 *  args: utmp record that is the reboot record, head pointer
 */
void reboot_logs(struct Node** head, struct utmp *record){

    Node* current = *head;
    while(current!= NULL){
        current->data.ut_time = record->ut_time;
        current = current->next;
    }
}

/*
 *  delete_logs -- deletes the entire log list
 *  args: head pointer to list
 */
  void delete_logs(Node** head) {
    Node* current = *head;
    Node* next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    *head = NULL;
}
/*
 *	main method, program entry
 *  process args and calls main function read_wtmp_file
 */

int main(int ac, char *av[]){
    char *info = WTMP_FILE;
    char *username;
    int e_flag = 0;
    for(int i = 1; i< ac; i++){ /* process args */
		if(strcmp(av[i], "-f")== 0 && i + 1 <ac){
			info = av[i+1];
			i++; /* skip next args since it's the filepath */
		}else if(strcmp(av[i], "-e")== 0){
			e_flag = 1;
		}else if(av[i][0] == '-'){ /* bad input */
            fprintf(stderr, "Unknown option: %s\n", av[i]);
            exit(EXIT_FAILURE);
        }
        else{
			username = av[i]; /* set username */
		}
	}
    if(username ==NULL){
        fprintf(stderr,"You must enter a username to the program with [-f filepath] [e] username\n");
        exit(EXIT_FAILURE); /* fail on no username */
    }
    read_wtmp_file(info, username, e_flag); /* process the file */
    return 0;
}

/*
 *  process_record -- reads the utmp record, and calls the linked list helper methods to process the utmp record 
 *  based on what kind of record it is. 
 *  args: current utmp record, username, head, earliest login
 */

void process_record(struct utmp* currRecord, char *username, Node** head, struct utmp** earliest_login){
    if(currRecord->ut_type == USER_PROCESS){ /* record is log in */
        //* keep track of earliest log in */
        if(*earliest_login == NULL || currRecord->ut_time< (*earliest_login)->ut_time){
            *earliest_login = currRecord;
        }
        if(strncmp(currRecord->ut_name, username,UT_NAMESIZE)==0){ /* check username */
            Node *matching_node = find_and_remove_matching_record(head, currRecord);
            if(matching_node != NULL){
                print_session_info(currRecord, &matching_node->data);  /* print user log in records */
                free(matching_node);  /* delete node */
            }else{
                print_session_info(currRecord, NULL); /* user still logged in but still need to print record */
            }
        }else{
            /* record could be implied log in */
            add_or_update_dead_process(head,currRecord); 
        }
    }else if(currRecord->ut_type == DEAD_PROCESS){ 
        add_or_update_dead_process(head,currRecord); /* record log out */
    }else if(currRecord->ut_type == BOOT_TIME){
        reboot_logs(head,currRecord); /* update log out records */
        latest_reboot = currRecord->ut_time;
    }else if(currRecord->ut_type == RUN_LVL){
        latest_shutdown = currRecord->ut_time; 
        delete_logs(head); /* delete history of log outs since there was a shutdown */
    }
}

/*
 *  read_wtmp_file -- calls bufferlib helper functions to open the file, and read the file starting from the last utmp record to the first, 
 *  and processes these records. After reading the file, it prints the earliest log in record and the file name, and prints the
 *  buffer efficiency if the user passed in the -e flag
 *  args: file to open, the username, and whether the user selected the -e flag
 */

void read_wtmp_file(char *info, char *username, int e_flag){
    int utmpfd;        /* read from this descriptor */
    struct utmp* currRecord;
    Node* head = NULL;
    struct utmp* earliest_login = NULL;

    if ((utmpfd = utmp_open(info)) == -1) /* open file */
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    int num_records = utmp_len(); /* get num utmp records in the file */

    for(int i = num_records -1; i>=0; i--) /* read last record to first */
    {
        currRecord = utmp_getrec(i); /* get record */
        process_record(currRecord,username,&head, &earliest_login); /* process each record */
    }
    if(earliest_login!=NULL){ 
        time_t login_time = earliest_login->ut_time;
        char login_time_str[32];
        strftime(login_time_str, sizeof(login_time_str), "%a %b  %-d %H:%M:%S %Y ", localtime(&login_time));
        printf("\n");
        printf("%s begins %s\n", info, login_time_str); /* print earliest log in */
    }
    if(e_flag==1){ /* if user wants to get buffer efficiency */
        int a[] = {0,0};
        utmp_stats(a);
        fprintf(stdout, "%d records read, %d buffer misses\n",a[0],a[1]);
    }
    utmp_close(utmpfd); /* close the file */
}

/*
 *  print_session_info -- calculates the session time and displays the session to the user
 *  args: the login record and the logout record. 
 */

void print_session_info(struct utmp *login_record, struct utmp *logout_record) {
    time_t login_time = login_record->ut_time;
    time_t logout_time = logout_record ? logout_record->ut_time : time(NULL);
    if(logout_record== NULL){ /* edge cases */
        if (latest_reboot!= 0){
            logout_time = latest_reboot;
        }if(latest_shutdown!=0){
            logout_time = latest_shutdown;
        }
    }
    double duration = difftime(logout_time, login_time); /* calculates session length */

    int hours = (int)(duration / 3600);
    int minutes = (int)((duration - (hours * 3600)) / 60);

    char login_time_str[32];
    char logout_time_str[32];
    
    strftime(login_time_str, sizeof(login_time_str), "%a %b  %-d %H:%M", localtime(&login_time)); //formats log in time
    if (logout_record) {
        strftime(logout_time_str, sizeof(logout_time_str), "%H:%M", localtime(&logout_time));
        printf("%-8.8s %-8.8s     %-16.16s %s - %s  (%02d:%02d)\n",
               login_record->ut_user, login_record->ut_line, login_record->ut_host, login_time_str, logout_time_str, hours, minutes);
    } else if(latest_reboot != 0){
        printf("%-8.8s %-8.8s     %-16.16s %s - crash (%02d:%02d)\n",
               login_record->ut_user, login_record->ut_line, login_record->ut_host, login_time_str, hours, minutes);
    } else if(latest_shutdown != 0){
        printf("%-8.8s %-8.8s     %-16.16s %s - down (%02d:%02d)\n",
               login_record->ut_user, login_record->ut_line, login_record->ut_host, login_time_str, hours, minutes);
    }
    else {
        printf("%-8.8s %-8.8s     %-16.16s %s - still logged in\n",
               login_record->ut_user, login_record->ut_line, login_record->ut_host, login_time_str);
    }
}
