#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
char *options;
int global_args_count;
int found = 0; // It is a global flag

// is_process_id_valid -> Uses "ps -p" to check if process id exists or not
int is_process_id_valid(pid_t pid) {
    char operation[50];
    sprintf(operation, "ps -p %d > /dev/null 2>&1", pid);
    return system(operation) == 0;
}

// is_process_id_zombie -> ps -o stat= -p to check if a process ID is defunct (zombie) by comaping its output(status) using strcmp
int is_process_id_zombie(pid_t pid) {
    char operation[50];
    sprintf(operation, "ps -o stat= -p %d", pid);
    FILE* fp = popen(operation, "r");
    if (!fp) {
        perror("failed using popen in is_process_id_zombie...");
        exit(1);
    }

    char status[10];
    if (fgets(status, sizeof(status), fp) == NULL) {
        fprintf(stderr, "Can't retrieve process status for PID %d\n", pid);
        pclose(fp);
        exit(1);
    }
    pclose(fp);
    // To Check if the process status indicates a defunct (zombie) process
    if (strncmp(status, "Z", 1) == 0) {
        return 1; // Process is defunct
    } else {
        return 0; // Process is not defunct
    }
}

// continue_paused_processes -> To resume (send SIGCONT) to paused processes
void continue_paused_processes() {
    // To open the file containing paused process IDs
    FILE *file = fopen("paused_processes.txt", "r");
    if (file == NULL) {
        perror("Unable to open file");
        exit(1);
    }
    char line[20];
    // Read process IDs from the file and send SIGCONT to each of them
    while (fgets(line, sizeof(line), file)) {
        pid_t pid = atoi(line);
    // Checking if the process is a zombie before sending SIGCONT
    if (!is_process_id_zombie(pid))
    {
        // Send SIGCONT signal to the process if not zombie
        printf("PID is : %d \n",pid);
        if (kill(pid, SIGCONT) == -1) {
            perror("Unable to send SIGCONT");
            // Continue sending SIGCONT to other processes
            // If one fails,  handles error differently
        } else {
            printf("Sent SIGCONT to process with PID %d\n", pid);
        }
    }else{
            printf("Ignoring process with PID %d as it is a zombie process\n", pid);
         }
    }

    fclose(file);
        // Delete the paused_processes.txt file
    if (remove("paused_processes.txt") != 0) {
        perror("Error deleting file");
    } else {

    }
}

// pause_process -> To pause a process using SIGSTOP and then storing those paused processes in a file named "paused_processes.txt"
void pause_process(pid_t process_id) {
    // Pause the process using SIGSTOP
    if (kill(process_id, SIGSTOP) == -1) {
        perror("Unable to pause process");
        exit(1);
    }
    printf("PID %d paused successfully.\n", process_id);

    // For storing the process ID in a file named paused_processes.txt
    int fd = open("paused_processes.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Unable to open file");
        exit(1);
    }

    char pid_str[20];
    sprintf(pid_str, "%d\n", process_id);

    if (write(fd, pid_str, strlen(pid_str)) == -1) {
        perror("Unable to write to file");
        exit(1);
    }

    close(fd);
}

// printall_zombie_descendants -> To recursively traverse the process tree and print all zombie descendants
void printall_zombie_descendants(pid_t process_id_3) {
    FILE *fp;
    char operation[50];
    sprintf(operation, "pgrep -P %d", process_id_3);
    fp = popen(operation, "r");
    if (fp == NULL) {
        printf("Unable to run pgrep command\n");
        exit(1);
    }
    char child_pid_str[10];
    while (fgets(child_pid_str, sizeof(child_pid_str), fp) != NULL) {
        pid_t child_pid = atoi(child_pid_str);
        if (is_process_id_zombie(child_pid)) {
            found = 1;
            printf("%d\n", child_pid);
        }
        //Using recursion to traverse the descendants of the current child process
        printall_zombie_descendants(child_pid);
    }
    pclose(fp);
}

// print_grandchildrens -> To list the PIDs of all the grandchildren of a given process ID
void print_grandchildrens(pid_t process_id) {
    // List all the child processes of the given process ID
    FILE *fp;
    char operation[50];
    sprintf(operation, "pgrep -P %d", process_id);
    fp = popen(operation, "r");
    if (fp == NULL) {
        printf("Unable to run pgrep command\n");
        exit(1);
    }

    char child_pid_str[10];
    printf("Grandchildren processes of process %d:\n", process_id);
    while (fgets(child_pid_str, sizeof(child_pid_str), fp) != NULL) {
        pid_t child_pid = atoi(child_pid_str);
        // List all the child processes of each child process (i.e grandchildren)
        char grandchild_command[50];
        sprintf(grandchild_command, "pgrep -P %d", child_pid);
        FILE *grandchild_fp = popen(grandchild_command, "r");
        if (grandchild_fp == NULL) {
            printf("Unable to run pgrep command\n");
            exit(1);
        }

        char grandchild_pid_str[10];
        while (fgets(grandchild_pid_str, sizeof(grandchild_pid_str), grandchild_fp) != NULL) {
            found = 1; // Set found flag to true
            pid_t grandchild_pid = atoi(grandchild_pid_str);
            printf("%d\n", grandchild_pid);
        }
        pclose(grandchild_fp);
    }
    pclose(fp);
}


// For listing all sibling of given process_id we need to use two functions -> print_parent_pid and print_sibling_processes
// 1. print_parent_pid -> To get the parent process ID (PPID) of a given process ID
pid_t print_parent_pid(pid_t pid) {
    char operation[50];
    sprintf(operation, "ps -o ppid= -p %d 2>/dev/null", pid); // Redirect stderr to /dev/null
    FILE* fp = popen(operation, "r");
    if (!fp) {
        perror("failing using  in print_parent_pid");
        exit(1);
    }

    char output[10];
    if (fgets(output, sizeof(output), fp) == NULL) {
        fprintf(stderr, "Unable to retrieve PPID for given process ID\n");
        pclose(fp);
        exit(1);
    }
    pclose(fp);

    // get the PPID from the output
    return atoi(output);
}

// 2. print_sibling_processes ->  To list all the sibling processes of a given process ID
void print_sibling_processes(pid_t pid) {
    pid_t parent_pid = print_parent_pid(pid);

    // List all processes with the same PPID
    printf("Sibling processes of process %d:\n", pid);
    char operation[50];
    sprintf(operation, "pgrep -P %d", parent_pid);
    FILE* fp = popen(operation, "r");
    if (!fp) {
        perror("failed using popen in print_sibling_processes");
        exit(1);
    }

    char child_pid_str[10];
    while (fgets(child_pid_str, sizeof(child_pid_str), fp) != NULL) {
        pid_t child_pid = atoi(child_pid_str);
        // To exclude the given process ID (i.e itself) from the list
        if (child_pid != pid) {
            found = 1; // Set found flag to true
            printf("%d\n", child_pid);
        }
    }
    pclose(fp);
}

// print_direct_descendants -> To list all direct descendants of given process_id
void print_direct_descendants(pid_t process_id_2) {
    FILE *fp;
    char operation[50];
    sprintf(operation, "pgrep -P %d", process_id_2);
    fp = popen(operation, "r");
    if (fp == NULL) {
        printf("Unable to run pgrep command\n");
        exit(1);
    }

    char child_pid_str[10];
    printf("Direct descendants of process %d:\n", process_id_2);
    while (fgets(child_pid_str, sizeof(child_pid_str), fp) != NULL) {
        found = 1; // Set found flag to true
        pid_t child_pid = atoi(child_pid_str);
        printf("%d\n", child_pid);
    }

    pclose(fp);
}

// print_non_direct_descendants -> To list all non direct descendents of given process_id
void print_non_direct_descendants(pid_t process_id_1, int count) {
    FILE *fp;
    char operation[50];
    char pid_str[10];
    sprintf(pid_str, "%d", process_id_1);
    sprintf(operation, "pgrep -P %s", pid_str);
    fp = popen(operation, "r");
    if (fp == NULL) {
        printf("Uable to run pgrep command\n");
        exit(1);
    }

    char child_pid_str[10];
    while (fgets(child_pid_str, sizeof(child_pid_str), fp) != NULL) {
        pid_t child_pid = atoi(child_pid_str);
        if (count > 1) {
                found =1;
            printf("%d ", child_pid);
        }
        print_non_direct_descendants(child_pid, count + 1);
    }

    pclose(fp);
}
// is_part_of_PTree -> To check if a given process_id belong to the process tree rooted at root_process
int is_part_of_pstree(pid_t process_id, pid_t root_process) {
    if (process_id == root_process)
        return 1;

    char operation[50];
    // Constructing the command to get the PPID of the given PID and redirecting stdoutput and error to /dev/null
    sprintf(operation, "ps -o ppid= -p %d 2>/dev/null", process_id); // Redirect stderr to /dev/null
    FILE* fp = popen(operation, "r"); // Execute the command and read its output
    if (!fp) {
        perror("failed using popen in is_part_of_pstree");
        return 0;
    }

    char output[10];
        if (fgets(output, sizeof(output), fp) == NULL) {
        // perror("Error reading output");
        fprintf(stderr, "Unable to retrieve information for given process ID\n" );
                pclose(fp); // Close the file pointer
        return 0;
    }
    /* fgets(output, sizeof(output), fp); // To read the output of the cli */
    pclose(fp); // Close the file pointer

    // Extract the PPID from the output
    int ppid = atoi(output);

    if (ppid == root_process)
        return 1;
    else if (ppid == 1)
        return 0;
    else
        return is_part_of_pstree(ppid, root_process);
}

int main(int argc, char *argv[]) {
    if (argc!= 3 && argc != 4 ) {
        printf("Usage: %s [process_id] [root_process] [options]\n", argv[0]);
        return 0;
    }

    // declaring process_id and root_process and  converting them from a string to an integer using atoi
    pid_t process_id = atoi(argv[1]);
    pid_t root_process = atoi(argv[2]);
    // To check if provided process IDs are valid before performing different operations on them
    if (!is_process_id_valid(process_id) || !is_process_id_valid(root_process)) {
        printf("process ID(s) provided are either invalid or process is not running or it does not exists.\n");
        return 1;
    }
    global_args_count = argc;
    if (is_part_of_pstree(process_id, root_process)) {
        printf("PID: %d, PPID: ", process_id);

        // To get process_id of parent process
        char operation[50];
        sprintf(operation, "ps -o ppid= -p %d", process_id);
        FILE* fp = popen(operation, "r");
        if (!fp) {
            perror("failed using popen in is_part_of_pstree");
            return 1;
        }
        char output[10];
        fgets(output, sizeof(output), fp);
        pclose(fp);

        // To list the process_id of parent process obtained above
        printf("%d\n", atoi(output));
    } else {
        printf("Does not belong to the process tree\n");
         return 0; // Stop if a process_id does not belong to process tree rooted at root process.
    }
    if (argc == 4) {
    options = argv[3];

    if(strcmp(options, "-xn") == 0){
    pid_t process_id_1 = atoi(argv[1]);
    print_non_direct_descendants(process_id_1, 1);
    if (!found) {
        printf("No non-direct descendants found...\n");
    }
    }
    else if (strcmp(options, "-xd") == 0){
    pid_t process_id_2 = atoi(argv[1]);
    print_direct_descendants(process_id_2);
    if (!found) {
        printf("No direct descendants found...\n");
    }
    }
    else if (strcmp(options, "-xs") == 0){
    pid_t process_id = atoi(argv[1]);
    // Check if it is able to get its parent
    if (print_parent_pid(process_id) == -1) {
        fprintf(stderr, "Invalid process ID or process is not running not able to get its parent\n");
        return 1;
    }
    print_sibling_processes(process_id);
    if (!found) {
        printf("No sibling processes found...\n");
    }

    }
    else if  (strcmp(options, "-xg") == 0){
        pid_t process_id = atoi(argv[1]);
        // List the PIDs of all the grandchildren of the given process ID
        print_grandchildrens(process_id);
        if (!found) {
        printf("No grandchildren found for given process_id...\n");
    }
    }
    else if (strcmp(options, "-xz") == 0){
    // Parse command line argument
    pid_t process_id = atoi(argv[1]);
    // Traverse the process tree and print all zombie descendants
    printall_zombie_descendants(process_id);
    // If no zombie descendants are found, print a message
    if (!found) {
        printf("No zombie descendants found...\n");
    }
    }
    else if (strcmp(options, "-zs") == 0) {
    // Parse command line argument
    pid_t process_id = atoi(argv[1]);
    // Check if the process is defunct or not and print the status
    if (is_process_id_zombie(process_id)) {
        printf("PID %d is defunct (zombie).\n", process_id);
    } else {
        printf("PID %d is not defunct (not a zombie).\n", process_id);
    }
    }else if (strcmp(options, "-xt") == 0) {

    // Parse command line argument
    pid_t process_id = atoi(argv[1]);
    // Check if the process is a zombie
    if (is_process_id_zombie(process_id)) {
    printf("PID %d is a zombie, so cannot be paused.\n", process_id);
    return 0;
    }
    // Pause the given process using SIGSTOP
    pause_process(process_id);
    }
    else if (strcmp(options, "-xc") == 0) {
         continue_paused_processes();
    }
    else if (strcmp(options, "-rp") == 0) {
    // Send SIGKILL signal to the process
    if (kill(process_id, SIGKILL) == -1) {
        perror("Unable to send SIGKILL");
        return 1;
    }
    printf("SIGKILL was sent to process_id with PID %d\n", process_id);
    }
    else if (strcmp(options, "-pr") == 0){
    // Send SIGKILL signal to the process
    if (kill(root_process, SIGKILL) == -1) {
        perror("Unable to send SIGKILL");
        return 1;
    }
    printf("SIGKILL was sent to root_process with PID %d\n", root_process);
    }
    else{
        printf("Invalid Option Try again!");
    }
    }
    return 0;
}