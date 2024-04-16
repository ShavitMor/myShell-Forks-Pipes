#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(){
    int pipe_fd[2];
    pid_t pid;
    pid_t pid2;


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
       
        fprintf(stderr,"(child1>redirecting stdout to the write end of the pipe…)\n");
        close(STDOUT_FILENO);
        dup(pipe_fd[1]);
        close(pipe_fd[0]);
        close(pipe_fd[1]); 

        char *argv[] = {"ls", "-l", NULL};
        fprintf(stderr,"(child1>going to execute cmd: …)\n");
        
         if (execvp("ls",argv) == -1) {
                perror("execvp");
                _exit(EXIT_FAILURE);
        };

        

    }   
    else{
        fprintf(stderr,"(parent_process>created process with id: %d)\n",pid);

        fprintf(stderr,"(parent_process>closing the write end of the pipe…)\n");
        
        close(pipe_fd[1]);
        pid2=fork();

        
        if(pid2==-1){
            fprintf(stderr,"fork error");
            exit(EXIT_FAILURE);
        }

        if(pid2==0){
            
            close(STDIN_FILENO);
            fprintf(stderr,"(child2>redirecting stdin to the read end of the pipe…)\n");
            dup(pipe_fd[0]);
            close(pipe_fd[0]); 
            close(pipe_fd[1]); 

            char *argv[] = {"tail", "-n","2", NULL};
            fprintf(stderr,"(child2>going to execute cmd: …)\n");
            execvp("tail", argv);
        }
        else{

            fprintf(stderr,"(parent_process>closing the read end of the pipe…)\n");
            close(pipe_fd[0]);

            int status;
            int status2;
            fprintf(stderr,"(parent_process>waiting for child processes to terminate…)\n");
            if (waitpid(pid, &status, 0) == -1) {
                        fprintf(stderr, "pid problem: %s\n");
                        exit(EXIT_FAILURE);
            }

            if (waitpid(pid2, &status2, 0) == -1) {
                        fprintf(stderr, "pid problem: %s\n");
                        exit(EXIT_FAILURE);
            }

            fprintf(stderr,"(parent_process>exiting…)\n");
            exit(EXIT_SUCCESS);

        }



       

    }

    return 0;
}