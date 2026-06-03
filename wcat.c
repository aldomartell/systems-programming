#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FILE *fp;
    char line[4096]; 
    if (argc == 1) {
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        fp = fopen(argv[i], "r");
        if (fp == NULL) {
            printf("wcat: cannot open file\n");
            return 1;
        }
        
        while (fgets(line, sizeof(line), fp) != NULL) {
            fputs(line, stdout);
        }
        
        fclose(fp);
    }
    
    return 0;
}