#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char* argv[]){
    FILE *fp;
    char line[4096];

    if(argc < 2){
        printf("wgrep: searchterm [file ...]\n");
        return 1;
    }
    char*word = argv[1];
    
    //if no file detected so we read from  stdin as directed on the instructions
    if(argc == 2){
        while(fgets(line, sizeof(line), stdin)  !=NULL){
            if(strstr(line, word) != NULL){
                printf("%s", line);
            }
        }
        return 0;
    }
    
    for(int i = 2; i<argc; i++){
        fp = fopen(argv[i], "r");
         if (fp == NULL) {
            printf("wgrep: cannot open file\n");
            return 1;
        }
            while(fgets(line, sizeof(line), fp)!= NULL){
                if(strstr(line, word) !=NULL){
                    printf("%s", line);
                }
        }
        fclose(fp);
    }

    return 0;

}