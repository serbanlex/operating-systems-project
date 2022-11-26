/*Programul primeste 2 argumente: 
1) path catre un director
2) optiune ('-a', '-u', '-nc').
Cerinte:
    1. Test de argumente.
    2. Parcurgere nerecursiva de director
    3. Identificati fisierele cu extensia .c
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

enum AccessIdentity{
    user,
    group,
    others
};

int ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void print_name(char *file_name){
    printf("> File name: %s\n\n", file_name);
}

void print_uid(struct stat file_stats){
    printf("UID: %d \n\n", file_stats.st_uid);
}

void print_dimension(struct stat file_stats){
    printf("Dimension: %lld Bytes\n", file_stats.st_size);
}

void print_access_for_identity(enum AccessIdentity identity, struct stat file_stats){
    int read_mask, write_mask, execute_mask;
    char identity_string[20] = "";
    switch(identity){
        case user:
            strcpy(identity_string, "User");
            read_mask = S_IRUSR;
            write_mask = S_IWUSR;
            execute_mask = S_IXUSR;
            break;
        case group:
            strcpy(identity_string, "Group");
            read_mask = S_IRGRP;
            write_mask = S_IWGRP;
            execute_mask = S_IXGRP;
            break;
        case others:
            strcpy(identity_string, "Others");
            read_mask = S_IROTH;
            write_mask = S_IWOTH;
            execute_mask = S_IXOTH; 
    }
    int status_mode = file_stats.st_mode;
    char *read = (status_mode & read_mask) ? "DA" : "NU";
    char *write = (status_mode & write_mask) ? "DA" : "NU";
    char *execute = (status_mode & execute_mask) ? "DA" : "NU";

    printf("%s:\nRead - %s\nWrite - %s\nExec - %s\n\n", identity_string, read, write, execute);
}

void print_accesses(struct stat file_stats){ 
    print_access_for_identity(user, file_stats);
    print_access_for_identity(group, file_stats);
    print_access_for_identity(others, file_stats);
}

void print_links_number(struct stat file_stats){ 
    printf("Number of links: %hu\n", file_stats.st_nlink);
}

void get_file_path_without_extension(char *file_path, char *file_path_without_extension){
    strcpy(file_path_without_extension, file_path);
    int index_of_extension_start = strlen(file_path) - 2;
    file_path_without_extension[index_of_extension_start] = '\0';
}

enum{
    read_end=0,
    write_end=1
};

void compile_file(char *file_path, int calculate_quality_grade){
    // int pipe_file_descriptors[2];
    // int process_pipe;

    // if((process_pipe = pipe(pipe_file_descriptors) < 0)){
    //     perror("Error creating pipe\n");
    //     exit(1);
    // }

    int pid = fork();
    
    if (pid == -1){
        char error_msg[256];
        sprintf(error_msg, "Couldn't fork current process in order to compile file %s", file_path);
        perror(error_msg);
        exit(-1);
    }

    if (pid == 0){
        // close(pipe_file_descriptors[read_end]);

        char file_executable[255] = ""; 
        get_file_path_without_extension(file_path, file_executable);
        strcat(file_executable, "_executable");
        // dup2(pipe_file_descriptors[write_end], 2);
        printf("INFO(PID %d): From a child process, started compiling the program named '%s', executable file: '%s'. \n", getpid(), file_path, file_executable);

        execlp("gcc", "gcc", "-Wall", "-o", file_executable, file_path, NULL);

        exit(-1);
    }
    else{
        int state;
        int waited_pid = waitpid(pid, &state, WUNTRACED);
        if(WIFEXITED(state)){
            printf("INFO(PID %d): Child process %d, intended to compile file %s, exited with status %d. \n", getpid(), waited_pid, file_path, WEXITSTATUS(state));
            // if(calculate_quality_grade){
            //     close(pipe_file_descriptors[write_end]);
            //     int pid_2;
            //     if ((pid_2 = fork() < 0)){
            //         perror("fork filter");
            //         exit(-1);
            //     };
            //     if (pid_2 == 0) {
            //         printf("INFO(PID %d): Din copil lansam un proces de grep cu filtre error|warning\n", getpid());
            //         execl("grep", "grep", "error|warning");
            //         perror("execl grep");
            //         exit(-1);
            //     }
            //     else{
            //         char buffer[256];
            //         int warnings = 0, errors = 0;
            //         while(read(process_pipe, buffer, 256)){
            //             if (strstr(buffer, "error")){
            //                 errors++;
            //             }
            //             else if (strstr(buffer, "warning")){
            //                 warnings++;
            //             }
            //         }
            //         int nota = 0;
            //         if (errors) {
            //             nota = 1;
            //         }
            //         else if (warnings >= 10){
            //             nota = 2;
            //         }
            //         else {
            //             nota = 2 + 8 * (10 - warnings) / 10;
            //         }

            //     }
            // }
            }
    }
}

int option_exists(char *input, char desired_option){
    for(int i = 1; i <= strlen(input); i++){
        if (input[i] == desired_option)
            return 1;
    }
    return 0;
}

void do_parse_options(char *file_name, char *input, struct stat file_stats, char *file_path){
    if (input[0] != '-'){
        printf("Invalid option format. Should start with '-' character.\n");
        exit(-1);
    }
    
    for(int i = 1; i <= strlen(input); i++){
        char option = input[i];
        switch(option){
            case 'a':
                print_accesses(file_stats);
                break;
            case 'n': 
                print_name(file_name);
                break;
            case 'u': 
                print_uid(file_stats);
                break;
            case 'd':
                print_dimension(file_stats);
                break; 
            case 'c':
                print_links_number(file_stats);
                break;
        }
    }
}

void parse_options(char *file_name, char *input, struct stat file_stats, char *file_path){
    if (option_exists(input, 'g')){
        int should_calculate_quality_grade = option_exists(input, 'p');
        compile_file(file_path, should_calculate_quality_grade);
    }

    int pid = fork();
    
    if (pid == -1){
        char error_msg[256];
        sprintf(error_msg, "Couldn't fork current process in order to parse the given options.");
        perror(error_msg);
        exit(-1);
    }

    if (pid == 0){
        printf("INFO(PID %d): From a child process, started parsing the given options and executing the desired features.\n", getpid());
        do_parse_options(file_name, input, file_stats, file_path);
        exit(0);
    }
    else{
        int state;
        int terminated_child_pid = waitpid(pid, &state, WUNTRACED);
        if(WIFEXITED(state)){
            printf("INFO(PID %d): Child process %d, intended to parse the given options, has ended with exit status %d. \n", getpid(), terminated_child_pid, WEXITSTATUS(state));
        }
    }
}

void print_separator(){
    printf("\n------------------------\n\n");
}

void create_sym_link(char *file_path, struct stat file_stats, int enable_logging){
    char file_path_without_extension[255] = "";
    get_file_path_without_extension(file_path, file_path_without_extension);
    symlink(file_path, file_path_without_extension);
    if(enable_logging)
        printf("INFO(PID %d): Created symlink `%s` for file `%s` \n", getpid(), file_path_without_extension, file_path);
}

void check_if_sym_link_creation_needed(struct stat file_stats, char *file_path, int max_number_of_kb){
    int pid = fork();

    if (pid == -1){
        char error_msg[256];
        sprintf(error_msg, "Couldn't fork current process in order to check for symlink creation for file %s \n", file_path);
        perror(error_msg);
        exit(-1);
    }

    if (pid == 0){
        printf("INFO(PID %d): In a child process, proceeding to check whether we make a symlink for file '%s' or not. \n", getpid(), file_path);

        int file_size_in_kb = file_stats.st_size / 1000; // st_size is the number in bytes => for kilobytes we divide

        if (file_size_in_kb < max_number_of_kb){
            printf("INFO(PID %d): Size of the file '%s' (%d kb) is less than %d kb. Initiating symlink creation. \n", getpid(), file_path, file_size_in_kb, max_number_of_kb);
            create_sym_link(file_path, file_stats, 1);
        }
        else{
            printf("INFO(PID %d): Size of the file '%s' (%d kb) is bigger than %d kb. No need for making symlink. \n", getpid(), file_path, file_size_in_kb, max_number_of_kb);
        }
        exit(0);
    }
    
    else{
        printf("INFO(PID %d): From the parent process, initializing symlink creation need checking for file '%s' (has to be max %d kb to be created). \n", getpid(), file_path, max_number_of_kb);
        int state;
        int terminated_child_pid = waitpid(pid, &state, WUNTRACED);
        if(WIFEXITED(state)){
            printf("INFO(PID %d): Child process %d, intended to check symlink for file '%s', exited with status %d. \n", getpid(), terminated_child_pid, file_path, WEXITSTATUS(state));
        }
    }
}

DIR* open_dir_with_checks(char *dir_name){
    struct stat lstat_of_dir;
    lstat(dir_name, &lstat_of_dir);

    if(lstat(dir_name, &lstat_of_dir) == -1){
        char error_msg[256];
        sprintf(error_msg, "Error checking lstat of directory `%s`", dir_name);
        perror(error_msg);
        exit(-1);
    }

    DIR *directory = opendir(dir_name);
    if (directory == NULL){
        perror("Could not enter directory");
        exit(-1);
    }

    return directory;
}

// gcc -Wall -o lab lab.c
int main(int argc, char *argv[]){
    if (argc != 3){
        printf("The program must have 2 arguments: 1 - dir path, 2 - option (-a, -u, -nc)\n");
        exit(-1);
    }

    char *dir_name = argv[1];
    DIR *directory = open_dir_with_checks(dir_name);

    char *options = argv[2];
    
    struct dirent *directory_entry;
    while ( (directory_entry = readdir(directory)) ){
        char *file_name = directory_entry->d_name;
        char file_path[256];
        sprintf(file_path, "%s/%s", argv[1], file_name);

        struct stat file_stats;
        lstat(file_path, &file_stats);

        if(S_ISREG(file_stats.st_mode) && ends_with(file_path, ".c")){
            check_if_sym_link_creation_needed(file_stats, file_path, 100);
            parse_options(file_name, options, file_stats, file_path);
            print_separator();
        }
    }
    closedir(directory);

}

