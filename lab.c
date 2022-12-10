#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

enum AccessIdentity{
    user,
    group,
    others
};

enum PipeEnds{
    read_end=0,
    write_end=1
};

char *accepted_options = "nuadcgp";


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


void print_separator(){
    printf("\n------------------------\n\n");
}


void print_name(char *file_name){
    printf("> File name: %s\n\n", file_name);
}


void print_uid(struct stat file_stats){
    printf("> UID: %d \n\n", file_stats.st_uid);
}


void print_dimension(struct stat file_stats){
    printf("> Dimension: %lld Bytes\n", file_stats.st_size);
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
    char *read = (status_mode & read_mask) ? "YES" : "NO";
    char *write = (status_mode & write_mask) ? "YES" : "NO";
    char *execute = (status_mode & execute_mask) ? "YES" : "NO";

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


void get_executable(char *file_path, char *file_executable) {
    get_file_path_without_extension(file_path, file_executable);
    strcat(file_executable, "_executable");
}


int get_quality_grade(int warnings, int errors) {
    int grade = 0;
    if (errors) {
        grade = 1;
    }
    else
        if (warnings >= 10){
            grade = 2;
        }
        else {
            grade = 2 + 8 * (10 - warnings) / 10;
        }
    return grade;
}


void wait_for_compiling_process_to_end(const char *file_path, int compiling_pid) {
    int compiling_process_state;
    int terminated_compiling_pid = waitpid(compiling_pid, &compiling_process_state, WUNTRACED);

    if(terminated_compiling_pid == -1){
        perror("gcc process execution failed");
    }

    if(WIFEXITED(compiling_process_state)){
        printf("INFO(PID %d): Child process %d, intended to compile file %s, exited with status %d. \n",
               getpid(), terminated_compiling_pid, file_path, WEXITSTATUS(compiling_process_state));
    }
}


void compile_file(char *file_path, int should_calculate_quality_grade){
    // the case for normal compiling
    if(!should_calculate_quality_grade){
        int gcc_compiling_pid = fork();
    
        if (gcc_compiling_pid == -1){
            char error_msg[256];
            sprintf(error_msg, "Couldn't fork current process in order to compile file %s", file_path);
            perror(error_msg);
            exit(-1);
        }

        if (gcc_compiling_pid == 0){
            char file_executable[255] = "";
            get_executable(file_path, file_executable);
            printf("INFO(PID %d): In a child process, started compiling the program named '%s', executable file: '%s'."
                   " \n", getpid(), file_path, file_executable);
            execlp("gcc", "gcc", "-Wall", "-o", file_executable, file_path, NULL);
            exit(-1);
        }
        else
            wait_for_compiling_process_to_end(file_path, gcc_compiling_pid);
    }
    // the case for compiling with quality grade calculation
    else{
        int pipe_gcc_to_filter[2], pipe_filter_to_parent[2];
        
        if(pipe(pipe_gcc_to_filter) < 0){
            perror("pipe gcc to filter");
        }
        if(pipe(pipe_filter_to_parent) < 0){
            perror("pipe filter to parent");
        }

        int gcc_compiling_pid = fork();
    
        if (gcc_compiling_pid == -1){
            char error_msg[256];
            sprintf(error_msg, "Couldn't fork current process in order to compile file %s", file_path);
            perror(error_msg);
            exit(-1);
        }

        if (gcc_compiling_pid == 0){
            close(pipe_gcc_to_filter[read_end]);
            close(pipe_filter_to_parent[read_end]);
            close(pipe_filter_to_parent[write_end]);

            char file_executable[256] = "";
            get_executable(file_path, file_executable);

            printf("INFO(PID %d): In a child process, started compiling the program named '%s', executable file: '%s'."
                   " \n", getpid(), file_path, file_executable);


            if(dup2(pipe_gcc_to_filter[write_end], STDERR_FILENO) < 0) {
                perror("dup2 pipe-gcc-filter over stderr");
            }// setting stderr to be written in the pipe (compiling warnings/errors output on stderr)

            execlp("gcc", "gcc", "-Wall", "-o", file_executable, file_path, NULL);
            exit(-1);
        }
        else{
            printf("INFO(PID %d): From the parent process, initializing another child process for calculating "
                   "quality of the code, continuously using the compilation output...\n", getpid());
            
            int filter_pid = fork();

            if (filter_pid == -1){
                perror("fork filter");
                exit(-1);
            }

            if (filter_pid == 0) {
                printf("INFO(PID %d): In a child process, using filter(grep) with error|warning filters.\n", getpid());
                close(pipe_gcc_to_filter[write_end]);
                close(pipe_filter_to_parent[read_end]);

                dup2(pipe_gcc_to_filter[read_end], STDIN_FILENO); // grep takes input from stdin
                dup2(pipe_filter_to_parent[write_end], STDOUT_FILENO); // grep would normally output to stdout
                close(pipe_gcc_to_filter[read_end]);
                close(pipe_filter_to_parent[write_end]);

                execlp("grep", "grep", "-E", "error|warning", NULL);
                perror("execl grep");
                exit(-1);
            }
            else{
                close(pipe_gcc_to_filter[write_end]);
                close(pipe_gcc_to_filter[read_end]);
                close(pipe_filter_to_parent[write_end]);

                char buffer[256];
                int warnings = 0, errors = 0;

                while(read(pipe_filter_to_parent[read_end], buffer, sizeof(buffer))){
                    if (strstr(buffer, "error")){
                        errors++;
                    }
                    else if (strstr(buffer, "warning")){
                        warnings++;
                    }
                }
                close(pipe_filter_to_parent[read_end]); // done with reading

                int filter_process_state;
                int terminated_filter_pid = waitpid(filter_pid, &filter_process_state, WUNTRACED);
                if(terminated_filter_pid == -1){
                    perror("filtering process");
                }

                if(WIFEXITED(filter_process_state)){
                    printf("INFO(PID %d): Child process %d, intended to grep errors and warnings, exited "
                           "with status %d. \n", getpid(), terminated_filter_pid, WEXITSTATUS(filter_process_state));
                }

                printf("> Quality grade: %d\n", get_quality_grade(warnings, errors));

                wait_for_compiling_process_to_end(file_path, gcc_compiling_pid);
            }     
        }
    }
}


bool option_is_accepted(char option){
    if(strchr(accepted_options, option))
        return true;
    return false;
}


bool option_is_active(char *given_options, char searched_option){
    for(int i = 1; i <= strlen(given_options); i++){
        if (given_options[i] == searched_option)
            return true;
    }
    return false;
}


void check_options_validity(const char *input) {
    if (input[0] != '-'){
        printf("ERROR: Invalid option format. Second argument, the options, should start with '-' character.\n");
        exit(-1);
    }
    if(strlen(input) == 1){
        printf("No options given. Nothing to do.\n");
        exit(0);
    }
    for(int i = 1; i <= strlen(input); i++){
        char option = input[i];
        if(!option_is_accepted(option)){
            printf("ERROR: Unknown option '%c'.\n", input[i]);
            exit(-1);
        }
    }
}


void execute_options(char *file_name, char *input, struct stat file_stats){
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


void parse_options(char *file_name, char *given_options, struct stat file_stats, char *file_path){
    if (option_is_active(given_options, 'g')){
        int should_calculate_quality_grade = option_is_active(given_options, 'p');
        compile_file(file_path, should_calculate_quality_grade);
    }

    int option_execution_pid = fork();

    if (option_execution_pid == -1){
        char error_msg[256];
        sprintf(error_msg, "Couldn't fork current process in order to parse the given options.");
        perror(error_msg);
        exit(-1);
    }

    if (option_execution_pid == 0){
        printf("INFO(PID %d): In a child process, started executing the remaining given options, if any.\n", getpid());
        execute_options(file_name, given_options, file_stats);
        exit(0);
    }
    else{
        int state;
        int terminated_option_execution_child_pid = waitpid(option_execution_pid, &state, WUNTRACED);

        if(terminated_option_execution_child_pid == -1){
            perror("Process execution for executing given options failed");
        }

        if(WIFEXITED(state)){
            printf("INFO(PID %d): Child process %d, intended to execute the remaining given options, has ended with the "
                   "exit status %d. \n", getpid(), terminated_option_execution_child_pid, WEXITSTATUS(state));
        }
    }
}


void create_sym_link(char *file_path){
    char file_path_without_extension[255] = "";
    get_file_path_without_extension(file_path, file_path_without_extension);
    symlink(file_path, file_path_without_extension);
    printf("INFO(PID %d): Created symlink `%s` for file `%s` \n", getpid(), file_path_without_extension, file_path);
}


void create_symlink_if_file_size_bigger_than(struct stat file_stats, char *file_path, int max_size_in_kb){
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

        if (file_size_in_kb < max_size_in_kb){
            printf("INFO(PID %d): Size of the file '%s' (%d kb) is less than %d kb. Initiating symlink creation. \n", getpid(), file_path, file_size_in_kb, max_size_in_kb);
            create_sym_link(file_path);
        }
        else{
            printf("INFO(PID %d): Size of the file '%s' (%d kb) is bigger than %d kb. No need for making symlink. \n", getpid(), file_path, file_size_in_kb, max_size_in_kb);
        }
        exit(0);
    }
    
    else{
        printf("INFO(PID %d): From the parent process, initializing symlink creation need checking for file '%s' (has to be max %d kb to be created). \n", getpid(), file_path, max_size_in_kb);
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
        sprintf(error_msg, "Error checking lstat of file or directory `%s`", dir_name);
        perror(error_msg);
        exit(-1);
    }

    if(!S_ISDIR(lstat_of_dir.st_mode)){
        printf("ERROR: Second argument must be a directory. Specified file '%s' is not a directory.", dir_name);
    }

    DIR *directory = opendir(dir_name);
    if (directory == NULL){
        perror("Could not enter directory");
        exit(-1);
    }

    return directory;
}


void check_argument_count(int argc) {
    if (argc != 3){
        printf("ERROR: Not enough arguments. The program must have 2 arguments: 1 - dir path, 2 - option (-a, -u, -nc)\n");
        exit(-1);
    }
}


bool is_c_file(const char *file_path, struct stat *file_stats) {
    return S_ISREG((*file_stats).st_mode) && ends_with(file_path, ".c");
}


int main(int argc, char *argv[]){
    check_argument_count(argc);
    char *dir_name = argv[1];
    char *given_options = argv[2];
    check_options_validity(given_options);

    DIR *directory = open_dir_with_checks(dir_name);

    struct dirent *directory_entry;
    while ( (directory_entry = readdir(directory)) ){
        char *file_name = directory_entry->d_name;
        char file_path[256];
        sprintf(file_path, "%s/%s", argv[1], file_name);

        struct stat file_stats;
        lstat(file_path, &file_stats);

        if(is_c_file(file_path, &file_stats)){
            create_symlink_if_file_size_bigger_than(file_stats, file_path, 100);
            parse_options(file_name, given_options, file_stats, file_path);
            print_separator();
        }
    }
    closedir(directory);

}

