#include<stdio.h> 
#include<stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h> 
#include <string.h>
#include <pthread.h>

#define BUFF_SIZE  256
#define EXIT_SUCCESS 0
#define EXIT_FAIL 1

char * path[BUFF_SIZE] = {"/bin", "/usr/bin", NULL};
int paths_num = 2;


typedef struct{//struct to pass data into each thread, the arguments and where to send the output
    char *args[BUFF_SIZE];
    char *outfile;
} cmd_t;

void error(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

char*search_path(char*cmd){//we need to go through each path folder and checks if the command exists there
    static char full_path[BUFF_SIZE];
    for (int i= 0; i< paths_num; i++){
        snprintf(full_path, BUFF_SIZE, "%s/%s", path[i], cmd);
        if (access(full_path, X_OK)== 0)
        return full_path;
    }
    return NULL;
}

void *run_cmd(void*arg) {//now for each thread forks a child process and runs the command
    cmd_t *cmd = (cmd_t *)arg;
    int rc = fork();
    if (rc< 0) { 
        error(); 
        return NULL; 
    }

    if (rc == 0) {//child
        if (cmd->outfile!= NULL) {
            int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd <0){ 
                error(); 
                exit(1);
            }
            dup2(fd, STDOUT_FILENO); //stdout foes to file
            dup2(fd, STDERR_FILENO); //as well as stderr
            close(fd);
        }
        char *exec_path = search_path(cmd->args[0]);//we look for the binary in the path list
        if (exec_path == NULL) {
            error(); 
            exit(1); 
        }

        execv(exec_path, cmd->args);//we replace child process with the command
        error();
        exit(1);
    }

    waitpid(rc, NULL, 0);//thread waits for its own child to finish 
    return NULL;
}

int main(int argc, char *argv[]){
    FILE *input = stdin;
    int batch_mode = 0;

    if (argc == 2){//if the file passed, run in batch mode
        input =fopen(argv[1], "r");
        if (input == NULL){
            error();
            exit(1);
        } batch_mode = 1;
    }

    else if (argc >2){//if more than one arg, error
        error();
        exit(1);
    }

    char *line = NULL;
    size_t size = 0;

    while(1){
        if(!batch_mode)
            printf("wish> ");
        if(getline(&line, &size, input) == -1 || feof(input))//read a line, exit if error
            exit(0);

        line[strcspn(line, "\n")] = '\0';//strip the newline at the end
        if (strlen(line) == 0) 
            continue;//skip blank lines

        char *segments[BUFF_SIZE];//split line by & in order to find parallel commands
        int nseg = 0;
        char *linecopy = strdup(line);
        char *rest = linecopy;
        char *seg;

        while((seg =strsep(&rest, "&"))!=NULL)
            segments[nseg++] = strdup(seg);
        free(linecopy);

        pthread_t threads[BUFF_SIZE];
        cmd_t cmds[BUFF_SIZE];
        int nthreads = 0;

        for (int i = 0; i <nseg; i++){//now we loop through each segment separated by & 
            char *s= segments[i];

            while (*s == ' ' || *s == '\t') //skip leading spaces
                s++;
            if (strlen(s) == 0) { 
                free(segments[i]); 
                continue; 
            }
            char *tmp =strdup(s);//make a copy of the segment so we dont mess up with the original
            char *trest =tmp;
            char *first=strsep(&trest, " \t");//grab the first word, the command name
            while (first && strlen(first) == 0)//skip any empty extra spaces
                first =strsep(&trest, " \t");
            
            if (strcmp(first, "exit") == 0) {// if user typed exit
                char *extra = strsep(&trest, " \t");//we check if there is anything after exit
                while (extra &&strlen(extra) == 0) //skip extra spaces again..
                    extra = strsep(&trest, " \t");
                if (extra)//if we identify something after exit, thats an error
                    error();
                else 
                    exit(0);
                    free(tmp); 
                    free(segments[i]);
                    continue;
            }

            if (strcmp(first, "cd") == 0){//if user typed cd
                char*dir = strsep(&trest, " \t");//get directory argument
                while (dir && strlen(dir)== 0)//skip empty space  
                    dir =strsep(&trest, " \t");
                char *extra = dir ? strsep(&trest, " \t") : NULL;//check for extra args  
                while (extra && strlen(extra) == 0) 
                    extra = strsep(&trest, " \t");
                if (!dir || extra) // no dir given, error
                    error();
                else if (chdir(dir)!= 0) //if try to change directory
                    error();
                    free(tmp); 
                    free(segments[i]);
                    continue;   
            }

            if (strcmp(first, "path") == 0){//if user typed path 
                paths_num = 0;
                char *p;
                while ((p= strsep(&trest, " \t"))!= NULL) {//grab each new path 
                    if (strlen(p) == 0) 
                        continue;
                        path[paths_num++]= strdup(p);
                }
                path[paths_num]= NULL;
                free(tmp); 
                free(segments[i]);
                continue;
            } free(tmp);

            cmd_t *cmd = &cmds[nthreads];//points to the next available cmd slot
            cmd->outfile = NULL;//no output file by default
            int nargs = 0;

            char *redir = strstr(s, ">");//look for > in the command
            if (redir) {
                *redir = '\0'; //we cut the string at >, so the left side is the command
                char *file_part = redir + 1;//filename
                
                if (strstr(file_part, ">")){ //if two > appears, error
                    error(); 
                    free(segments[i]); 
                    continue; 
                }

                char *fname = strsep(&file_part, " \t");//get filename
                while (fname && strlen(fname) == 0) //skip spaces
                    fname = strsep(&file_part, " \t");
                if (fname == NULL){ //if no file, error
                    error(); 
                    free(segments[i]); 
                    continue; 
                }

                
                char *extra = strsep(&file_part, " \t");//anything extra after filename is an error
                while (extra && strlen(extra) == 0) 
                    extra = strsep(&file_part, " \t");
                if (extra){ 
                    error(); 
                    free(segments[i]); 
                    continue; 
                }

                cmd->outfile = strdup(fname);//save the output filename
            }

            char *scopy = strdup(s);//string copy, so strsep does not destroy the original 
            char *srest = scopy;
            char *token;//each word we pull out one by one
            while ((token = strsep(&srest, " \t"))!= NULL) {//keep grabbing words until nothing is left
                if (strlen(token) == 0) 
                    continue;
                    cmd->args[nargs++] = strdup(token);//store the word as the next argument
            }
            cmd->args[nargs] = NULL;
            free(scopy);//free the copy now

            if (nargs == 0) { 
                error();
                free(segments[i]); 
                continue; 
            }

            pthread_create(&threads[nthreads], NULL, run_cmd, cmd);//launch a thread for this command
            nthreads++;
            free(segments[i]);
        }
            
        for (int i = 0; i< nthreads; i++)//wait for every thread to finish before moving to the next line
            pthread_join(threads[i], NULL);
        }
        free(line);
        return 0;

}
