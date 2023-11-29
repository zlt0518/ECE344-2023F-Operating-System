#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

int isNumber(char str[])
{
    for(int i = 0; str[i]!= '\0'; i++)
    {
        if (isdigit(str[i]) == 0) return 0;
    }
    return 1;
}

int main() {

    DIR* dir = opendir("/proc");

    struct dirent *dirEntry;

    if(dir == NULL)
    {
        perror( "can't open /proc" );
    }
    else
    {
        //print the title
        printf("%5s CMD\n","PID");

        while((dirEntry = readdir(dir)) != NULL)
        {
            //get process number directory
            if(isNumber(dirEntry->d_name))
            {
                printf("%5s ", dirEntry->d_name);
                char filename[100];
                strcpy(filename,"/proc/");
                char statusfile[] = "/status";
                strcat(filename,dirEntry->d_name);
                strcat(filename,statusfile);
                
                int fd = open(filename,O_RDONLY, 0);
                if (fd<0)
                {
                    perror( "can't open file" );
                }

                int file_size;     // Number of bytes returned
                char data[60];    // Retrieved data

                int num_bytes = 60;     // Number of bytes to read

                //read the file
                file_size = read(fd, data, num_bytes);
                
                if(file_size<0)
                {
                    perror( "can't read file" );
                }

                for(int i = 6; data[i]; i++)
                {
                    printf("%c", data[i]);
                    if(data[i]== '\n') break;
                }

                //close the file
                int close_file = close(fd);
                if(close_file<0)
                {
                    perror( "can't close file" );
                }
                    
            }
        }
            
    }

    closedir(dir);  //close the directory

    return 0;
}

