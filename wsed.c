#include<stdio.h>
#include<string.h>
#include  <stdlib.h>
#include<getopt.h>
// prints  info when someone  asks for help
void print_help(){
    printf("Usage: wsed [-m mode] [initial] [final] [file]\n");
    printf("-m  mode\n");
    printf("\t\"substitution\" or \"translation\". Default: substitution.\n");
    printf("initial & final\n");
    printf("\t in substitution, all occurrences of initial in [file] are replaced with final\n");
    printf("\t in translation, each character in initial is replaced with the the corresponding character from final in [file]\n");
}

//we go through the line looking for that pattern so we find it and replace it
void substitution(FILE *file, const char  *pattern, const char *replacement){
    char *line = NULL;
      size_t len = 0;
    //each line
   while (getline(&line, &len, file) != -1) {
        char *position = line;
        char *match;
        //then here, keep searching for the pattern until we cant find it anymore
        while ((match = strstr(position, pattern)) != NULL) {
            fwrite(position, 1, match - position, stdout);//so every time the pattern is found, then print the replacement basically
            fputs(replacement, stdout);
            position = match + strlen(pattern);
        }
        //whatever is left at the end of the line
        fputs(position, stdout);

   }
   free(line);
}

void translation(FILE *file, const char*source, const char *dest){
    char *line= NULL;
    size_t len  = 0;
    ssize_t read;
    size_t dest_len = strlen(dest);
    
    while ((read  =getline(&line, &len, file)) != -1) {
        for (size_t i = 0; i < (size_t)read; i++) {
            //we chekc if this character is in the string source
            const char *found = strchr(source, line[i]);
            if (found != NULL) {//then if it is, we need to find the index position, 
                size_t index = found - source;
                if (index < dest_len) {
                    line[i] = dest[index];//then replace it at the same position in dest
                }
            }
        }
        fwrite(line, 1, read, stdout);
     }
     free(line);
}

int main(int argc, char *argv[]){
    char mode[20] = "substitution";
    int opt;
    int count = 0;
    //getopt to work on the command line arguments, mode, help, and -
    while ((opt = getopt(argc, argv, "m:h-:")) != -1) {
        switch (opt) {
            case 'm'://mode, so strncpy allow us to copy and avoid overwriting the memory, 
                strncpy(mode, optarg, sizeof(mode) - 1);
                mode[sizeof(mode) - 1] = '\0';
                count++;
                break;
            case '-'://if using --help
            if (strcmp(optarg, "help") == 0) {
                    print_help();
                    return 0;
                } 
                else {
                    printf("wsed: [option] [val1] [val2] [file]\n");
                    return 1;
                }
            case '?':
                printf("wsed: [option] [val1] [val2] [file]\n");
                return 1;
            case 'h':
            //h is help basically, so we call print help function on the top
                print_help();
                return 0;
        }
    }
     if (count  >1) {
        printf("wsed: [option] [val1] [val2] [file]\n");
        return 1;
    }

     int remaining_args = argc - optind;

     if (remaining_args != 3) {
        printf("wsed: [option] [val1] [val2] [file]\n");
        return 1;
    }
    
    const char *val1 = argv[optind];
    const char *val2 = argv[optind + 1];
    const char *filename = argv[optind + 2];//3 remaining arguments val1 val2 and the file name

    if (strcmp(mode, "substitution") != 0 && strcmp(mode, "translation") != 0) {
        printf("wsed:unknown mode detected\n");
        printf("mode must be \"substitution\" or \"translation\"\n");
        return 1;
    }

    //so now we implement the right and proper function, based on the mode 

    if(strcmp(mode, "translation") == 0 && strlen(val1) != strlen(val2)){
        printf("wsed: Translation strings not equal length\n");
        return 1;
    }

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
         printf("wsed: cannot open file\n");
         return 1;
    }

     if (strcmp(mode, "substitution")== 0) {
         substitution(file, val1, val2);
     } else {
        translation(file, val1, val2);
    }
    fclose(file);
    return 0;
}//this was the most challeging task out of the 3 of the project1, but applying string manipulation functions for example, to find a pattern strstr was necessary are really important
//and i learned a lot applying the string functions, since it was basically 2 functions translation and substitution it was required string 