#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main (){
    printf("inainte de fork!\n");
    int pid = fork();
    if (pid == -1){
        printf("nasol\n");
        exit(-1);
    }

    if(pid == 0){
        printf("cod din copil\n");          
    }
    else {
        printf("cod din parinte\n");
    }

}