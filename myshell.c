#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include "LineParser.h"
#include <signal.h>

#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 20

typedef struct process{
        cmdLine* cmd;                         /* the parsed command line*/
        pid_t pid; 		                  /* the process id that is running the command*/
        int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
        struct process *next;	                  /* next process in chain */
    } process;

process *process_list = NULL;

typedef struct history {
    char cmdLine[2048];                   /* the unparsed command line */
} history;

history history_list[HISTLEN];
int history_newest = -1;
int history_oldest = -1;


int debugMode = 0; 


void updateProcessList(process **process_list) {
    process *currProcess = *process_list;
    while (currProcess) {
        int status;
        pid_t result = waitpid(currProcess->pid, &status, WNOHANG | WUNTRACED);
         if (result == -1) {
            currProcess->status = TERMINATED; 
        }
        else if (result > 0) {
            if (WIFCONTINUED(status))
                currProcess->status = RUNNING;
            else if (WIFSTOPPED(status)) 
                currProcess->status = SUSPENDED;
            else if (WIFEXITED(status) || WIFSIGNALED(status))
                currProcess->status = TERMINATED;
        }
        currProcess = currProcess->next;
    }
}

void updateProcessStatus(process* process_list, int pid, int status) {
    process *currProcess = process_list;
    while (currProcess) {
        if (currProcess->pid == pid) {
            currProcess->status = status;
            break; 
        }
        currProcess = currProcess->next;
    }
}


void freeProcessList(process* process_list){
    if(process_list){
    freeCmdLines(process_list->cmd);
    
    freeProcessList(process_list->next);
    free(process_list);
    }
}




void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    process *newProcess = (process*)malloc(sizeof(process));

    newProcess->cmd = cmd;
    newProcess->pid = pid;
    newProcess->status = RUNNING;
    newProcess->next = *process_list;
    *process_list = newProcess;
    
}

void deleteTerminated(process** process_list) {
    process *current = *process_list;
    process *prev = NULL;

    while (current != NULL) {
        if (current->status == TERMINATED) {
            if (prev == NULL) {
               
                *process_list = current->next;
                free(current);
                current = *process_list;
                } else {
                prev->next = current->next;
                free(current);
                current = prev->next;
            }
        } else {
            prev = current;
            current = current->next;
        }
    }
}

void addToHistory(const char* cmdLine) {
    if (history_newest == -1) {
        history_newest = 0;
        history_oldest = 0;
    } else {
        history_newest = (history_newest + 1) % HISTLEN;
        if (history_newest == history_oldest) {
            history_oldest = (history_oldest + 1) % HISTLEN;
        }
    }
    strncpy(history_list[history_newest].cmdLine, cmdLine, sizeof(history_list[history_newest].cmdLine));
}

void printHistory() {
    int i = history_oldest;
    int index = 1;
    while (i != history_newest) {
        printf("[%d] %s\n", index++, history_list[i].cmdLine);
        i = (i + 1) % HISTLEN;
    }
    printf("[%d] %s\n", index++, history_list[i].cmdLine);
}



void printProcessList(process** process_list) {
    updateProcessList(process_list);
    int index = 0;
    process *currProcess = *process_list;
    while (currProcess) {
        fprintf(stderr,"[%d] %d ", index++, currProcess->pid);
        if (currProcess->status == RUNNING)
            fprintf(stderr,"RUNNING ");
        else if (currProcess->status == SUSPENDED)
            fprintf(stderr,"SUSPENDED ");
        else if (currProcess->status == TERMINATED){
            fprintf(stderr,"TERMINATED ");
            
            }
        for (int i = 0; i < currProcess->cmd->argCount; i++)
            fprintf(stderr,"%s ", currProcess->cmd->arguments[i]);

        fprintf(stderr,"\n");
        currProcess = currProcess->next;
    }
    deleteTerminated(process_list);
}

 void execute(cmdLine *pCmdLine) {
    if(strcmp(pCmdLine->arguments[0],"cd")==0){
        if (chdir(pCmdLine->arguments[1]) != 0) {
            fprintf(stderr, "cd failed: %s\n");
            return EXIT_FAILURE;
        }
    }

    else if(strcmp(pCmdLine->arguments[0],"procs")==0)
    {
       
        printProcessList(&process_list);
    }
    else if(strcmp(pCmdLine->arguments[0],"history")==0)
    {
       
        printHistory();
    }
    else if (strcmp(pCmdLine->arguments[0], "!!") == 0) {
        
        char* cmdline = malloc(strlen(history_list[history_newest-1].cmdLine) + 1);
        if (history_newest == -1) {
            fprintf(stderr,"No history available.\n");
            return;
        }
        strcpy(cmdline, history_list[history_newest-1].cmdLine);
        cmdline[strcspn(cmdline, "\n")] = '\0';
        cmdLine* parsedCmd = parseCmdLines(cmdline);
        free(cmdline);
        if(parsedCmd->next){
            execute_pipe(parsedCmd);
            
        }
        else{
            execute(parsedCmd);
        }
        freeCmdLines(parsedCmd);
    }
    else if (pCmdLine->arguments[0][0] == '!') {
        int index = atoi(&pCmdLine->arguments[0][1]);
        if (index < 1 || index > HISTLEN || index > history_newest - history_oldest + 1) {
            fprintf(stderr,"Invalid history index.\n");
            return;
        }
        int histIndex = index % HISTLEN;
        char* cmdline = malloc(strlen(history_list[histIndex-1].cmdLine) + 1);
        strcpy(cmdline, history_list[(histIndex)-1].cmdLine);
        cmdline[strcspn(cmdline, "\n")] = '\0';
        cmdLine* parsedCmd = parseCmdLines(cmdline);
        free(cmdline);
        if(parsedCmd->next){
            execute_pipe(parsedCmd);
        }
        else{
            execute(parsedCmd);
        }
        freeCmdLines(parsedCmd);
    }
    else if (strcmp(pCmdLine->arguments[0], "wakeup") == 0) {
        pid_t pid = atoi(pCmdLine->arguments[1]);
        if (kill(pid, SIGCONT) == -1) {
            fprintf(stderr, "error continue proccess");
            return;
        }
        updateProcessStatus(process_list,pid,RUNNING);
        if(debugMode==1){
            fprintf(stderr,"Woke up process with PID %d\n", pid);
        }
    }
    else if (strcmp(pCmdLine->arguments[0], "nuke") == 0) {
        pid_t pid = atoi(pCmdLine->arguments[1]);
        if (kill(pid, SIGINT) == -1) {
            fprintf(stderr, "error terminate proccess");
            return;
        }
        updateProcessStatus(process_list,pid,TERMINATED);

        if(debugMode==1){
            fprintf(stderr,"Terminated process with PID %d\n", pid);
        }
    } 
     else if (strcmp(pCmdLine->arguments[0], "suspend") == 0) {
        pid_t pid = atoi(pCmdLine->arguments[1]);
        if (kill(pid, SIGTSTP) == -1) {
            fprintf(stderr, "error suspending proccess");
            return;
        }
        updateProcessStatus(process_list,pid,SUSPENDED);
        if(debugMode==1){
            fprintf(stderr,"Suspended process with PID %d\n", pid);
        }
    } 
    else{
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "error fork operation");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            if (pCmdLine->inputRedirect != NULL) {
                FILE *fileInput = fopen(pCmdLine->inputRedirect, "r");
                if (fileInput == NULL) {
                    fprintf(stderr, "error open file");
                    _exit(EXIT_FAILURE);
                }
                int fileDescriptor = fileno(fileInput);
                close(STDIN_FILENO); 
                dup(fileDescriptor); 
                close(fileDescriptor); 
                fclose(fileInput); 
            }

        

            if (pCmdLine->outputRedirect != NULL) {
                FILE *fileOutput = fopen(pCmdLine->outputRedirect, "w");
                if (fileOutput == NULL) {
                    fprintf(stderr, "error output file");
                    _exit(EXIT_FAILURE);
                }
                int fileDescriptor = fileno(fileOutput);
                close(STDOUT_FILENO);
                dup(fileDescriptor); 
                close(fileDescriptor); 
                fclose(fileOutput); 
            }


            if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
                perror("execvp");
                _exit(EXIT_FAILURE);
            }
        }
        else
        {
            int status;
            if(debugMode==1){
                fprintf(stderr,"%d\n",pid);
            }
            if(debugMode==1){
                fprintf(stderr,"%s\n",pCmdLine->arguments[0]);
            }
            addProcess(&process_list,pCmdLine,pid);
            if(pCmdLine->blocking==1){
                if (waitpid(pid, &status, 0) == -1) {
                    fprintf(stderr, "pid problem: %s\n");
                    exit(EXIT_FAILURE);
                }
            }
            
        }
    }
}

void execute_pipe(cmdLine *pCmdLine){
    int pipe_fd[2];
    pid_t pid;
    pid_t pid2;
    if(pCmdLine->outputRedirect)
        perror("cant redirect the left when using pipe");
    if(pCmdLine->next->inputRedirect)
        perror("cant redirect the right when using pipe");

    
    if (pipe(pipe_fd) == -1) {
        fprintf(stderr,"pipe error");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr,"(parent_process>forking…)\n");
    pid = fork();

    if(pid==-1){
        fprintf(stderr,"fork error");
        exit(EXIT_FAILURE);
    }

    if(pid==0){
       
        if (pCmdLine->inputRedirect != NULL) {
            FILE *fileInput = fopen(pCmdLine->inputRedirect, "r");
            if (fileInput == NULL) {
                fprintf(stderr, "error open file");
                _exit(EXIT_FAILURE);
            }
            int fileDescriptor = fileno(fileInput);
            close(STDIN_FILENO); 
            dup(fileDescriptor); 
            close(fileDescriptor); 
            fclose(fileInput); 
        }


        fprintf(stderr,"(child1>redirecting stdout to the write end of the pipe…)\n");
        close(STDOUT_FILENO);
        dup(pipe_fd[1]);
        close(pipe_fd[0]);
        close(pipe_fd[1]); 

        fprintf(stderr,"(child1>going to execute cmd: …)\n");
        
        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
                perror("execvp");
                _exit(EXIT_FAILURE);
            }
               

        

    }   
    else{
        fprintf(stderr,"(parent_process>created process with id: %d)\n",pid);

        fprintf(stderr,"(parent_process>closing the write end of the pipe…)\n");
        
        close(pipe_fd[1]);

        addProcess(&process_list,pCmdLine,pid);

        pid2=fork();

        
        if(pid2==-1){
            fprintf(stderr,"fork error");
            exit(EXIT_FAILURE);
        }

        if(pid2==0){

            if (pCmdLine->next->outputRedirect != NULL) {
                FILE *fileOutput = fopen(pCmdLine->next->outputRedirect, "w");
                if (fileOutput == NULL) {
                    fprintf(stderr, "error output file");
                    _exit(EXIT_FAILURE);
                }
                int fileDescriptor = fileno(fileOutput);
                close(STDOUT_FILENO);
                dup(fileDescriptor); 
                close(fileDescriptor); 
                fclose(fileOutput); 
            }



            close(STDIN_FILENO);
            fprintf(stderr,"(child2>redirecting stdin to the read end of the pipe…)\n");
            dup(pipe_fd[0]);
            close(pipe_fd[0]); 
            close(pipe_fd[1]); 

          
            fprintf(stderr,"(child2>going to execute cmd: …)\n");
             if (execvp(pCmdLine->next->arguments[0], pCmdLine->next->arguments) == -1) {
                perror("execvp");
                _exit(EXIT_FAILURE);
            }
        }
        else{
            addProcess(&process_list,pCmdLine->next,pid);

            fprintf(stderr,"(parent_process>closing the read end of the pipe…)\n");
            close(pipe_fd[0]);

            int status;
            int status2;
            fprintf(stderr,"(parent_process>waiting for child processes to terminate…)\n");
            if(pCmdLine->blocking==1){
            if (waitpid(pid, &status, 0) == -1) {
                        fprintf(stderr, "pid problem: %s\n");
                        exit(EXIT_FAILURE);
            }
            }
            if(pCmdLine->next->blocking==1){
            if (waitpid(pid2, &status2, 0) == -1) {
                        fprintf(stderr, "pid problem: %s\n");
                        exit(EXIT_FAILURE);
            }
            }
            fprintf(stderr,"(parent_process>exiting…)\n");
        }


    }

}


int main(int argc, char *argv[]) {


  
    cmdLine* parsedCmd=NULL;
    for(int i=1; i<argc; i++){
        if(strcmp(argv[i],"-d")==0){
            debugMode=1;
        }
    }


    while (1) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            if(debugMode==1){
                fprintf(stderr,"%s$ ", cwd);
            }
        } else {
            fprintf(stderr, "error getting cwd");
            exit(EXIT_FAILURE);
        }

        char input[2048];
        if (fgets(input, sizeof(input), stdin) == NULL) {
            exit(EXIT_FAILURE);
        }
       
        input[strcspn(input, "\n")] = '\0';
        if(strcmp(input, "!!") == 0){
            addToHistory(history_list[history_newest].cmdLine);         
        }
        else if (input[0]=='!')
        {
            char indexStr[strlen(input) - 1]; 
            strncpy(indexStr, &input[1], strlen(input) - 1); 
            indexStr[strlen(input) - 1] = '\0'; 
            int index = atoi(indexStr); 
            
            if(index>history_newest){
                perror("out of bounds of history");
            }
            else{
            addToHistory(history_list[index-1].cmdLine);
            }
        }
        else
        {
         addToHistory(input);

        }

        
        if (strcmp(input, "quit") == 0) {
            if(debugMode==1){
            fprintf(stderr,"exiting...\n");
            }
             
             freeProcessList(process_list);
             free(process_list);
             
            break;
        }
        


        parsedCmd = parseCmdLines(input);
        
        if(parsedCmd->next){
            execute_pipe(parsedCmd);
        }
        else{
            execute(parsedCmd);
        }
        freeCmdLines(parsedCmd);
       
    }

    freeCmdLines(parsedCmd);
    freeProcessList(process_list);
    return 0;
}