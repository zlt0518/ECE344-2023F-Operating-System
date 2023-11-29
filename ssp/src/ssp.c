#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

//global variable
unsigned int longest_process_name;
int ssp_num;
struct subprocess *ssp_list; //ptr to process array

//process struct
struct subprocess 
{
    pid_t pid;
    char name[256];
    int ssp_status;
};

int ssp_insert(char *name,int status, pid_t pid)
{
    //increase the process number 
    ssp_num+=1;

    //reallocat/malloc the array for increasing size  -> ok to use large size array instead 
    if(ssp_num == 1)
    {
        ssp_list = malloc(ssp_num * sizeof(struct subprocess));
    }
    else
    {
        ssp_list = realloc(ssp_list, ssp_num * sizeof(struct subprocess)) ;
    }
    
    int ssp_id_curr = ssp_num - 1;  

    //update the length for process name
    if(strlen(name)>longest_process_name)
    {
        longest_process_name = strlen(name);
    }

    //update the process name
    strcpy(ssp_list[ssp_id_curr].name,name);
    ssp_list[ssp_id_curr].ssp_status = status;

    ssp_list[ssp_id_curr].pid = pid;

    return ssp_id_curr;
}


void handle_signal(int signum) {

    if (signum != SIGCHLD) {
        perror("Error Signal");
    }

    int status;

    //wait for all child
    pid_t return_pid = waitpid(-1, &status, WNOHANG);//WNOHANG FLAG
    
    if (return_pid == 0)
    {
        return;
    }
    else if(return_pid == -1)
    {
        //no child process
        if (ECHILD){
            errno = 0;
            return;
        }
        else
        {
            exit(errno);    
        }
                
    }

    for (int i = 0; i<ssp_num; i++){

        if (return_pid == ssp_list[i].pid){
            
            //on the arrray process, no need to keep
            if (WIFSIGNALED(status))
            {
                ssp_list[i].ssp_status = WTERMSIG(status)+128;
            } 
            else if (WIFEXITED(status))
            {
                ssp_list[i].ssp_status = WEXITSTATUS(status);
            }  

            return;
        }
    }

    int orp_status = -1;
    if (WIFSIGNALED(status))
    {
        orp_status = WTERMSIG(status)+128;
    } 
    else if (WIFEXITED(status))
    {
        orp_status = WEXITSTATUS(status);
    }  

    ssp_insert("<unknown>",orp_status,return_pid);

}


//from lecture
void register_signal(int signum)
{
  struct sigaction new_action = {0};
  sigemptyset(&new_action.sa_mask);
  new_action.sa_handler = handle_signal;
  new_action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(signum, &new_action, NULL) == -1) {
    int err = errno;
    perror("sigaction");
    exit(err);
  }
}


void ssp_init() {

    longest_process_name = 3;
    ssp_num = 0;
    prctl(PR_SET_CHILD_SUBREAPER, 1);
    register_signal(SIGCHLD);
}

int ssp_create(char *const *argv, int fd0, int fd1, int fd2)
{
    // create a new child
    pid_t pid = fork();

    //ERROR for fork
    if (pid == -1)
    {
        return errno;
    }
    else if (pid == 0)
    {
        //child
        pid_t curr_pid =getpid();

        //dup2 for the child
        dup2(fd0,0);
        dup2(fd1,1);
        dup2(fd2,2);
        
        //close other fd
        char foldername[256];        
        sprintf(foldername, "%s%d%s", "/proc/", curr_pid, "/fd/");
        int process_fd = open(foldername, O_RDONLY);
        
        DIR* dir = fdopendir(process_fd);
        struct dirent *dirEntry;

        if(dir == NULL)
        {
            perror( "can't open /proc" );
        }
        else
        {
            while((dirEntry = readdir(dir)) != NULL)
            {
            //get process number directory
                if(dirEntry->d_type == DT_LNK)
                {
                    int fd_num = atoi(dirEntry->d_name);
                    //do not close fd 0,1,2 and itself
                    if((fd_num ==0)||(fd_num==1)||(fd_num==2)||(fd_num==process_fd)) 
                    {
                        continue;
                    }
                    else
                    {
                        close(fd_num);
                    }
                }   
            }   
        }

        closedir(dir);
        close(process_fd);

        execvp(argv[0],argv); //execute
        
        exit(errno); //exit for error

    }
    else
    {
        //parents
        int new_ssp_id = ssp_insert(argv[0],-1,pid);

        return new_ssp_id;
    }

    return -1;
}

int ssp_get_status(int ssp_id) {

    //the table shows its running
    if(ssp_list[ssp_id].ssp_status  == -1) 
    {
        //check status
        int status;
        pid_t return_pid = waitpid(ssp_list[ssp_id].pid, &status, WNOHANG); //WNOHANG FLAG

        if(return_pid == -1)
        {
            //error
            exit(errno);            
        }
        else if (return_pid>0)
        {
            //update ssp status if it exited or signaled
            if(WIFEXITED(status))
            {
                ssp_list[ssp_id].ssp_status = WEXITSTATUS(status);               
            }
            else if (WIFSIGNALED(status))
            {
                ssp_list[ssp_id].ssp_status = WTERMSIG(status)+128;
            }
        }
    }

    return ssp_list[ssp_id].ssp_status;

}

void ssp_send_signal(int ssp_id, int signum) {

    //check the status and update status after kill
    if(ssp_list[ssp_id].ssp_status == -1 && ssp_id<ssp_num)
    {
        kill(ssp_list[ssp_id].pid, signum);
        ssp_list[ssp_id].ssp_status = (signum + 128);    
    }

}

void ssp_wait() {

    for (int i = 0;i<ssp_num;i++)
    {
        if(ssp_list[i].ssp_status==-1)
        {
            int status;
            pid_t wait_pid = waitpid(ssp_list[i].pid,&status,0);

            if(wait_pid<0)
            {
                perror("waitpid");
            }

            //check the status and update status based on exit code or signal
            if(WIFEXITED(status))
            {
                ssp_list[i].ssp_status = WEXITSTATUS(status);
            }
            else if(WIFSIGNALED(status))
            {
                ssp_list[i].ssp_status = WTERMSIG(status) + 128;
            }

        }
    }

}

void ssp_print() {
    printf("%7s %-*s STATUS\n","PID",longest_process_name,"CMD");
    
    for (int i = 0;i<ssp_num;i++)
    {
        printf("%7d %-*s %d\n", ssp_list[i].pid,longest_process_name,ssp_list[i].name,ssp_list[i].ssp_status);
    }

    return;
}
