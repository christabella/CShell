/* final version... without lolcat :-( */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h> 
#include <fcntl.h> 

#include <sys/param.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <sys/ioctl.h> 
#include <time.h>

#define RESET_COLOR "\e[m" 
#define MAKE_RED "\e[1;31m"
#define MAKE_YELLOW "\e[1;33m"
#define MAKE_GREEN "\e[1;32m" 
#define MAKE_CYAN "\e[1;36m"
#define MAKE_PURPLE "\e[1;34m" 
#define MAKE_PINK "\e[1;35m"
#define TO_STRING(A) #A


struct file {
    char *name;
    time_t time;
    int unsigned long size;
};

/* for "list": qsort() compare functions */

int cmpsizefn (const void *a, const void *b) {
    struct file *a_ptr = (struct file *)(a);
    struct file *b_ptr = (struct file *)(b);
    return (a_ptr->size - b_ptr->size);
}

int cmpnamefn (const void *a, const void *b) {
    struct file *a_ptr = (struct file *)(a);
    struct file *b_ptr = (struct file *)(b);
    return strcmp(a_ptr->name, b_ptr->name);
}

int cmptimefn (const void *a, const void *b) {;
    struct file *a_ptr = (struct file *)(a);
    struct file *b_ptr = (struct file *)(b);
    return (difftime(a_ptr->time, b_ptr->time));
}

/* for "find": recursively walks through directory printing everything non-hidden */

static void find_files (const char *dirpathname, char *needle)
{
    DIR *d;

    d = opendir(dirpathname);

    if (! d) {
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not open current working directory.\n");
        exit (EXIT_FAILURE);
    }
    while (1) {
        struct dirent *entry;
        const char *entry_name;
        // "Readdir" gets subsequent entries from "d".
        entry = readdir (d);
        if (! entry) {
            // There are no more entries in this directory
            break;
        }

        entry_name = entry->d_name;
        if (strstr(entry_name, needle) != NULL)         
            printf (MAKE_CYAN"%s/%s\n", dirpathname, entry_name);

        if (entry->d_type & DT_DIR) {

            // Check that the directory is not "d" or d's parent. 
            if (strcmp(entry_name, "..") != 0 && strcmp(entry_name, ".") != 0) {
                int path_length;
                char path[PATH_MAX];
 
                /* int snprintf ( char * s, size_t n, const char * format, ... )
                Write formatted output to sized buffer; return num of chars written or negative number for encoding error. 
                Notice that only when this returned value is non-negative and less than n, the string has been completely written. */
                path_length = snprintf (path, PATH_MAX,
                                        "%s/%s", dirpathname, entry_name);
                if (path_length >= PATH_MAX) {
                    printf(MAKE_RED"ERROR: "MAKE_GREEN"Path length has got too long.\n");
                    exit (EXIT_FAILURE);
                }
                /* Recursively call "find_files" with the new path. */
                find_files(path, needle);
            }
        }
    }
    /* After going through all the entries, close the directory. */
    if (closedir(d)) {
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not close current working directory.\n");
        exit (EXIT_FAILURE);
    }
}


/* for "tree": scandir() compare and filter functions */

int treecmpsizefn (const struct dirent **a, const struct dirent **b) {
    // **a is a pointer to a dirent (directory entry) structs
    // (*a) dereferences the pointer and accesses the dirent struct itself

    int unsigned long a_size;
    int unsigned long b_size;

    int filedescriptor = -1;
    struct stat statbuffer;
    struct stat statbufferb;

    if (!(filedescriptor = open((*a)->d_name, O_RDONLY))) {
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not get file descriptor.\n"); 
        return 0;
    }

    fstat(filedescriptor, &statbuffer);
    a_size = (int unsigned long) statbuffer.st_size;

    close(filedescriptor);

    filedescriptor = -1;

    if (!(filedescriptor = open((*b)->d_name, O_RDONLY))) {
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not get file descriptor.\n"); 
        return 0;
    }

    fstat(filedescriptor, &statbufferb);
    b_size = (int unsigned long) statbufferb.st_size;

    return (a_size - b_size);
    
    close(filedescriptor);
}

int treecmptimefn (const struct dirent **a, const struct dirent **b) {;

    time_t a_time;
    time_t b_time;

    int filedescriptor = -1;
    struct stat statbuffer;
    struct stat statbufferb;

    if (!(filedescriptor = open((*a)->d_name, O_RDONLY))) {
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not get file descriptor.\n"); 
        return 0;
    }

    fstat(filedescriptor, &statbuffer);
    a_time = (int unsigned long) statbuffer.st_mtime;

    close(filedescriptor);

    filedescriptor = -1;

    if (!(filedescriptor = open((*b)->d_name, O_RDONLY))) {
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not get file descriptor.\n"); 
        return 0;
    }

    fstat(filedescriptor, &statbufferb);
    b_time = (int unsigned long) statbufferb.st_mtime;

    close(filedescriptor);
    
    return (difftime(a_time, b_time));
}

int treecmpnamefn (const struct dirent **a, const struct dirent **b) {
    const char *a_name = (*a)->d_name; 
    const char *b_name = (*b)->d_name;
    return strcmp(a_name, b_name);
}

int filter(const struct dirent *entry) {
    return entry->d_name[0] != '.';
}

/* for "tree": recursively scans directory, sorting with scandir() compare functions */

static void display_tree (const char *dirpathname, int levels, char *sortby, int level, char *prefix) {
    int i = 0;
    int n = 0;
    struct dirent **entries = NULL;
    /*
       int scandir(const char *dir, struct dirent ***namelist,
              int(*filter)(const struct dirent *),
              int(*compar)(const struct dirent **, const struct dirent **));

        The scandir() function scans the directory dir, filtered entries are 
        stored in strings sorted using qsort with compar(), and collected in array namelist

       @return the number of directory entries selected
       or -1 if an error occurs.

    */
    if ((sortby != NULL) && (sortby[0] != '\0')) {
        if (strcmp(sortby, "time") == 0) {
            n = scandir(dirpathname, &entries, filter, treecmptimefn);
        } else if (strcmp(sortby, "size") == 0) {
            n = scandir(dirpathname, &entries, filter, treecmpsizefn);
        } else if (strcmp(sortby, "name") == 0) {
            n = scandir(dirpathname, &entries, filter, treecmpnamefn);
        }
    } else {
        n = scandir(dirpathname, &entries, filter, NULL);
    }

    if (n < 0) {
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not scan current working directory.\n"); 
    } else {
        for (i = 0; i < n; i++) {
            /* printf( "%*s", level*2, "" );    <-- for a minimalist indent-only tree display */

            /* prefixing file names for pretty tree structure */
            printf("%s", prefix);
            if (i == n-1) {     // if last entry in directory
                printf("└");
            } else {
                printf("|");
            }
            printf("-- ");

            // DIRECTORY ALERT DIRECTORY ALERT DIRECTORY ALERT
            if (entries[i]->d_type & DT_DIR) {
                printf(MAKE_CYAN"%s\n"MAKE_PINK, entries[i]->d_name);

                char newpath[PATH_MAX];
                newpath[0] = '\0';
                strcat(newpath, dirpathname);
                strcat(newpath, "/");
                strcat(newpath, entries[i]->d_name);
            
                /* taking care of prefixing following entries */
                char newprefix[128];
                newprefix[0] = '\0';
                strcat(newprefix, prefix);
                if (i == n-1) {        
                    /*  last entry for this level already; you no longer need to prefix 
                        a trunk for subsequent entries  */
                    strcat(newprefix, "    "); 
                } else {
                    /*  keep the trunk  */
                    strcat(newprefix, "|   ");
                }

                if (level < levels) // haven't burst the maximum level of subdirectories to traverse 
                    display_tree(newpath, levels, sortby, level+1, newprefix);

            } else {
                printf(RESET_COLOR"%s\n"MAKE_PINK, entries[i]->d_name);

            }
            free(entries[i]);
        }
        free(entries);
    }

}

int main() {

    //variables
    char command[100];
    char *stringPtr;
    char *create = "create";
    char *delete = "delete";
    char *display = "display";
    char *list = "list";
    char *property = "property";
    char *propertyname = "property name";
    char *propertytime = "property time";
    char *propertysize = "property size";
    char *find = "find";
    char *tree = "tree";
    char *filename;
    char *ptr;

    /*  print welcome logo in rainbow (only if you have the ruby gem `lolcat` installed)
        otherwise, prints in a not-as-cool rainbow
            ___________           _________        ___      ___        
           /     _     \         /    _    \      /   \    /   \       
          /     /E|____/|       /    / \    |    /    /|  /    /|
         /     /EE|EEE|/       |    |  /___/|   /    /E|_/    /E| 
        |     |EE/         ____\    \ |EEE||   /    ____     /EE/
        |     |E/_____    /    /\    \|EEE|/  /    /\__/    /EE/
        |\     \_\    \  |    |__\    \      /    /EE//    /EE/
        |E\___________/| |\___________/|    /____/EE//____/EE/
        |E|EEEEEEEEEE|E| |E|EEE||EEEE|E|    |EEEE|E/ |EEEE|E/
         \|EEEEEEEEEE|/   \|EEEE||EEEE|/    |EEEE|/  |EEEE|/  
    */

    const char* cshASCII = "\n    ___________           _________        ___      ___        \n   /     _     \\         /    _    \\      /   \\    /   \\       \n  /     /E|____/|       /    / \\    |    /    /|  /    /|\n\e[1;32m /     /EE|EEE|/       |    |  /___/|   /    /E|_/    /E| \n|     |EE/         ____\\    \\ |EEE||   /    ____     /EE/\n\e[1;36m|     |E/_____    /    /\\    \\|EEE|/  /    /\\__/    /EE/\n|\\     \\_\\    \\  |    |__\\    \\      /    /EE//    /EE/\n\e[1;34m|E\\___________/| |\\___________/|    /____/EE//____/EE/\n|E|EEEEEEEEEE|E| |E|EEE||EEEE|E|    |EEEE|E/ |EEEE|E/\n\e[1;35m \\|EEEEEEEEEE|/   \\|EEEE||EEEE|/    |EEEE|/  |EEEE|/  \n"; 
    printf(MAKE_YELLOW"%s\n"RESET_COLOR, cshASCII);

    while(1) {
        printf(MAKE_PURPLE"\ncsh> "MAKE_YELLOW);
        fgets(command, MAX_INPUT/4, stdin); // take user input and save it in command

    //--------------------------------------------------------------------
    /*** Create ****/
        ptr = strstr(command, create);
        if (ptr != NULL)
        {
            stringPtr = command; // pointer now references command char[100]
            strsep(&stringPtr, " "); // discard "create", leaves behind filename + "\n" in stringPtr 
            filename = strsep(&stringPtr, "\n"); // char *filename points to filename without "\n" 
            FILE *file = fopen(filename, "w");
            if (!file) 
            {
                printf(MAKE_RED"ERROR: "MAKE_GREEN"Failed to create %s.\n"RESET_COLOR, filename);
                continue;
            }
            printf(MAKE_CYAN"Successfully created %s.\n", filename);
            continue;
        }

    //--------------------------------------------------------------------
    /*** Delete ****/
        ptr = strstr(command, delete);
        if (ptr != NULL)
        {
            stringPtr = command; // pointer now references command char[100]
            strsep(&stringPtr, " "); // discard "delete ", leaves behind filename + "\n" in stringPtr 
            filename = strsep(&stringPtr, "\n"); // char *filename points to filename without "\n" 
            FILE *file = fopen(filename, "r");
            if (!file) {
                printf(MAKE_RED"ERROR: "MAKE_GREEN"File does not exist.\n");
                continue;
            }
            remove(filename);
            printf(MAKE_CYAN"Successfully deleted %s.\n", filename);
            continue;        
        }

    //--------------------------------------------------------------------
    /*** Display ****/
        ptr = strstr(command, display);
        if (ptr != NULL)
        {
            stringPtr = command; // pointer now references command char[100]
            strsep(&stringPtr, " "); // discard "delete ", leaves behind filename + "\n" in stringPtr 
            filename = strsep(&stringPtr, "\n"); // chr points to filename without "\n" 
            FILE *file = fopen(filename, "r");
            if (!file) 
            {
                printf(MAKE_RED"ERROR: "MAKE_GREEN"File does not exist.\n");
                continue;
            }
            int c;
            printf(MAKE_CYAN"");
            while ((c = getc(file)) != EOF)
                putchar(c);
            fclose(file);
            continue;
        }


    //--------------------------------------------------------------------
    /*** List ****/

    /* Expects:
     * list
     * list property 
     * list property time
     * list property size
     * list property name
     * etc..
    */

        ptr = strstr(command, list);
        if (ptr != NULL)
        {
            char dirpathname[PATH_MAX];
            struct dirent *entryPtr; 
            DIR *dstreamPtr; // DIR is a data type that represents a directory stream
            unsigned int count = 0; 

            if (!getcwd(dirpathname, sizeof(dirpathname)) || !(dstreamPtr = opendir((const char*)dirpathname))) {
                printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not open current working directory.\n");
                continue;
            }

            // no need for property, just list file names
            if (strstr(command, property) == NULL) { 


                for (count = 0; (entryPtr = readdir(dstreamPtr)) != NULL; count++) { 
                    // if file is not hidden
                    if (entryPtr->d_name[0] != '.') { 

                        // if file is executable 
                        if (!access(entryPtr->d_name,X_OK)) { 
                            int filedescriptor = -1; 
                            struct stat statbuffer; // file status buffer
              
                            if (!(filedescriptor = open(entryPtr->d_name, O_RDONLY))) { 
                                printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not get file descriptor.\n"); 
                                continue;
                            } 
                             
                            fstat(filedescriptor, &statbuffer); 
                            
                            // if directory
                            if (S_ISDIR(statbuffer.st_mode)) { 
                                printf(MAKE_CYAN"%s\n"RESET_COLOR,entryPtr->d_name); 
                            } else {                                   
                                printf(MAKE_GREEN"%s\n"RESET_COLOR,entryPtr->d_name); 
                            } 
                            close(filedescriptor); 
                        } else { 
                            // No executable flag ON 
                            // Print it in black(default) 
                            printf(RESET_COLOR"%s\n",entryPtr->d_name); 
                        } 
                    }
                } 
            // need to list properties
            } else { 

                struct file *files = (struct file *) malloc(1024 * sizeof(struct file));

                // log file name, size, date last modified
                while ((entryPtr = readdir(dstreamPtr)) != NULL) { 
                    if (entryPtr->d_name[0] != '.' && entryPtr->d_name[0] != '(') {

                        int filedescriptor = -1; 
                        struct stat statbuffer; // file status buffer
          
                        if (!(filedescriptor = open(entryPtr->d_name, O_RDONLY))) { 
                            printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not get file descriptor.\n"); 
                            continue;
                        } 
                        files[count].name = entryPtr->d_name;
                        
                        fstat(filedescriptor, &statbuffer); 
                        files[count].time = statbuffer.st_mtime;
                        files[count].size = (int unsigned long) statbuffer.st_size;

                        close(filedescriptor); 
                        count++;
                    }
                }   

                if (strstr(command, propertysize) != NULL) {
                    qsort(files, count, sizeof(struct file), cmpsizefn);
                } else if (strstr(command, propertytime) != NULL) {
                    qsort(files, count, sizeof(struct file), cmptimefn);
                } else if (strstr(command, propertyname) != NULL) {
                    qsort(files, count, sizeof(struct file), cmpnamefn);
                }

                // actual_time_sec = difftime(actual_time,0); 

                for (int i = 0; i < count; i++) {
                    char timebuff[20]; 
                    char sizebuff[20]; 
                    struct tm *timeinfo;

                    timeinfo = localtime (&(files[i].time)); 
                    strftime(timebuff, 20, "%b %d %H:%M", timeinfo); 

                    // convert files[i].size to string
                    sprintf(sizebuff, "%lu", files[i].size);
                    strcat(sizebuff, " bytes");
                    printf(MAKE_CYAN"%-25s \e[1;34mSize: \e[1;32m%-20s \e[1;34mLast Modified: \e[1;35m%s\n", files[i].name, sizebuff, timebuff);
                    
                }
                free(files);
            }
            closedir(dstreamPtr);
            continue;
        }

    //--------------------------------------------------------------------
    /*** Find ****/

    /* Expects:
     * find .
     * find .txt 
     * find Sim
     * etc..
    */

        ptr = strstr(command, find);
        if (ptr != NULL)
        {
            stringPtr = command; // pointer now references command char[100]
            strsep(&stringPtr, " "); // discard "delete ", leaves behind search term+ "\n" in stringPtr 
            filename = strsep(&stringPtr, "\n"); // chr points to filename without "\n" 

            if (!filename) {
                printf(MAKE_RED"ERROR: "MAKE_GREEN"\"Find\" needs another argument.\n");
                continue;
            }

            char dirpathname[PATH_MAX];
            if (!getcwd(dirpathname, sizeof(dirpathname))) {
                printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not open current working directory.\n");
                continue;
            }
            
            find_files((const char *) dirpathname, filename);
            continue;
        }


    //--------------------------------------------------------------------
    /*** Tree ****/

    /* Expects:
     * tree
     * tree 1 
     * tree 1 time
     * tree size
     * tree name
     * etc..
    */

        ptr = strstr(command, tree);
        if (ptr != NULL)
        {

            char dirpathname[PATH_MAX];
            if (!getcwd(dirpathname, sizeof(dirpathname))) {
                printf(MAKE_RED"ERROR: "MAKE_GREEN"Could not open current working directory.\n");
                continue;
            }

            printf(MAKE_PINK".\n");
            char *argbuf;            
            int levels;            
            char sortby[20];

            stringPtr = command; // pointer now references command char[100]
            strsep(&stringPtr, " "); // discard "tree ", leaves behind [level + sortby] + "\n" in stringPtr 

            argbuf = strsep(&stringPtr, "\n"); // stringPtr has "\n" left, possible levels and/or sortby argbuf

            if (!argbuf) {
                display_tree(dirpathname, 999, "", 1, "");
                continue;
            }

            levels = atoi(argbuf); // atoi returns 0 if it can't find a number at the start of the string

            if (levels == 0) {
                levels = 999;
            }

            int i = 0;
            int j = 0;

            while (i != strlen(argbuf)) {
                if ((argbuf[i] >= 97) && (argbuf[i] <= 122)) {
                    sortby[j] = argbuf[i];
                    i++;
                    j++;
                } else {
                    i++;
                }
            }
            sortby[j] = '\0';

            display_tree(dirpathname, levels, sortby, 1, "");
            continue;

        }
    //--------------------------------------------------------------------
    //Wrong command
              
        printf(MAKE_RED"ERROR: "MAKE_GREEN"Command not recognized.\n");
        
            


    }//end while
}//end main
