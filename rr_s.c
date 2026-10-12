#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

// signal handler
void timerHandler(int signal) {
    (void)signal; // I use the void to avoid warnings on the gcc
}

// first we need to create a structure to store each process in for when we parse the tsv file
typedef struct {
    int id; // process identifier
    int priority; // 0-127
    char *program; // binary executable file
    char **args; // optional arguments for the binary executable file
    pid_t pid;
    int active; // 1 means process still needs schedule while 0 means it is finished
} Process;


int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Incorrect number of arguments. Give quantum number and tsv file name.\n");
        return 1;
    }

    int quantum = atoi(argv[1]);

    if (quantum <= 0) {
        printf("Quantum must be greater than 0 milliseconds\n");
        return 1;
    }

    // set up a SIGALRM handler for the scheduler timer
    struct sigaction action;

    action.sa_handler = timerHandler; // call the handler that was defined above
    sigemptyset(&action.sa_mask); // which signals should be blocked while the handler is running
    action.sa_flags = 0;

    if (sigaction(SIGALRM, &action, NULL) == -1) {
        printf("error on the sigaction");
        return 1;
    }

    // second we need to parse the tsv file and put that information into our previously declared structure
    char *filename = argv[2];

    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    // useful variables before we parse
    char line[1024]; // buffer
    Process *processes = NULL; // a list of all Process structs
    int processCount = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        
        Process process;

        // id
        char *token = strtok(line, "\t");
        process.id = atoi(token);

        // priority number
        token = strtok(NULL, "\t");
        process.priority = atoi(token);

        // priority number validation check
        if (process.priority < 0 || process.priority > 127) {
            printf("Invalid priority. Priority must be between 0 and 127.\n");
            fclose(file);
            return 1;
        }

        // executable program
        process.args = NULL; // arguments for the executable
        int argCount = 0;

        // now parse program name and all arguments for that executable
        while ((token = strtok(NULL, "\t\n")) != NULL) {

            // increase the size of the argument array inside the struct by 1 to have room for the argument
            process.args = realloc(process.args, sizeof(char *) * (argCount + 1));

            // actually puts the argument string into that space
            process.args[argCount] = strdup(token);

            // the first argument is also the executable name so we only do this on the first iteration of this loop
            if (argCount == 0) {
                process.program = strdup(token);
            }

            argCount++;
        }

        // add final NULL for exec so the future execvp call will know where to stop
        process.args = realloc(process.args, sizeof(char *) * (argCount + 1));
        process.args[argCount] = NULL;

        // pid does not exist yet because we haven't forked
        process.pid = -1;

        // every process begins "active", meaning it's not inactive or completed
        process.active = 1;

        // allocate enough space for another process
        if (processCount == 0) {
            processes = malloc(sizeof(Process));
        } else {
            processes = realloc(processes, sizeof(Process) * (processCount + 1));
        }

        processes[processCount] = process;
        processCount++;
    }

    fclose(file);

    // in the event there are no processes to execute
    if(processCount == 0){
        printf("No processes found in the tsv file\n");
        free(processes);
        return 1;
    }

    // third we ill begin every child process and have them wait for the scheduler to tell them what to do
    for(int i = 0; i < processCount; i++) {

        pid_t childPid = fork();

        if(childPid < 0) {
            printf("Error forking");
            return 1;
        }

        if(childPid == 0) { // child process

            raise(SIGSTOP); // child will immediately stop itself

            // this will be allowed to happen when the parent process later sens the SIGCONT signal
            execvp(processes[i].program, processes[i].args);

            // this will only execute if the exec fails
            printf("error on the execvp");
            exit(1);
        } else { // parent process

            // parent needs to store the PID of the child
            processes[i].pid = childPid;

            // wait for child process
            if (waitpid(childPid, NULL, WUNTRACED) == -1) { // this flag makes the wait call also return on a stop signal
                printf("error waiting for child to stop\n");
                return 1;
            } 
        }

    }

    // fourth we will implement the scheduler logic
    int remainingProcesses = processCount;
    int lastScheduled = -1;

    while(remainingProcesses > 0) {

        int highestPriotiry = 128;

        // find the highest-priority active process group
        for(int i = 0; i < processCount; i++) {

            if(processes[i].active && processes[i]. priority < highestPriotiry) {
                highestPriotiry = processes[i].priority;
            }
        }


        // now find the first active process in the highest priority group
        int currentProcess = -1;

        for(int offset = 1; offset <= processCount; offset++){

            // the circular behavior we want for the round robin algorithm within the same priority group
            int i = (lastScheduled + offset) % processCount;

            if(processes[i].active && processes[i].priority == highestPriotiry) {
                currentProcess = i;
                break;
            }
        }

        // we will now resume that selected child process
        if(currentProcess != -1) {

            // update our last scheduled process
            lastScheduled = currentProcess;
            
            // create a time for this process' quantum
            struct itimerval timer;

            timer.it_value.tv_sec = quantum / 1000;
            timer.it_value.tv_usec = (quantum % 1000) * 1000;

            // we only want the timer to go off once
            timer.it_interval.tv_sec = 0;
            timer.it_interval.tv_usec = 0;

            // start the timer
            if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
                printf("error on the setitimer");
                return 1;
            }

            // now finally resume the process
            if (kill(processes[currentProcess].pid, SIGCONT) == -1) {
                printf("error on the kill SIGCONT");
                return 1;
            }

            // now we want run the child process and wait to see what happens to this child process
            int status;
            pid_t result = waitpid(processes[currentProcess].pid, &status, WUNTRACED);

            // quantum expired while the process was still running
            if(result == -1 && errno == EINTR) {

                // stop the child
                if (kill(processes[currentProcess].pid, SIGSTOP) == -1) {
                    printf("error on the kill SIGSTOP");
                    return 1;
                }

                // confirm that it actually stopped
                result = waitpid(processes[currentProcess].pid, &status, WUNTRACED);
            }

            // works for both if the if statement is called or not
            if(result == -1){
                printf("error waiting");
                return 1;
            }

            // this cancels any remaining timer so the next process has a fresh timer
            struct itimerval stopTimer = {0};
            if (setitimer(ITIMER_REAL, &stopTimer, NULL) == -1) {
                printf("error stopping timer\n");
                return 1;
            }

            // now check what happened to the process
            if(WIFEXITED(status)) {

                processes[currentProcess].active = 0;
                remainingProcesses--;

            } else if(WIFSIGNALED(status)) {

                processes[currentProcess].active = 0;
                remainingProcesses--;

            } else if(WIFSTOPPED(status)) {

                // process is still active and will be schedules again so don't do anything here
            }
        }
    }

    // free all dynamically allocated memory (free the heap!)
    for (int i = 0; i < processCount; i++) {

        free(processes[i].program);
        for (int j = 0; processes[i].args[j] != NULL; j++) {
            free(processes[i].args[j]);
        }

        free(processes[i].args);
    }

    free(processes);

    return 0;
}
