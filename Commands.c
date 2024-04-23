#define _XOPEN_SOURCE 500 // Used in case of nftw
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

char *root_dir;
char *filename;
char *storage_dir;
char *options;
char *extension;
int srch_successful = 0;
int global_args_count;
int match_found=0;
/*
    Using this Function to do below tasks:-
        Task 1 :search filenames in root_dir and its subdirectories.
        Task 2 :move and copy file with given filename from root_dir to storage_dir based on options provided.
*/
/*
Below parameters are used
file_loc_path: A string representing the path of the current file being processed.
struct_stat: A pointer to a structure containing information about the current file.
filetype: An integer expressing the type of file.
ftwbuf: A pointer to a structure that has details of the current file being processed.
*/
int search_file(const char *file_loc_path, const struct stat *struct_stat, int filetype, struct FTW *ftwbuf) {
    if (filetype == FTW_F && strcmp(file_loc_path + ftwbuf->base, filename) == 0) // To check current file type being processed and compare it with filename
    {
        printf("File found at: %s\n", file_loc_path);

        if (global_args_count == 3) // To make it run for task1
        {
            srch_successful = 1;
            return 0;
        }

        char dest_path_loc[PATH_MAX];
        snprintf(dest_path_loc, PATH_MAX, "%s/%s", storage_dir, filename); // To make destination file path

        if (strcmp(options, "-cp") == 0) // Logic for copying file
        {
            int src_fd, dest_fd;
            char buffer[4096];
            ssize_t bytesRd, bytesWr;

            src_fd = open(file_loc_path, O_RDONLY); // opening file at source
            if (src_fd == -1) {
                perror("Error occuring while opening source file");
                exit(EXIT_FAILURE);
            }

            dest_fd = open(dest_path_loc, O_WRONLY | O_CREAT | O_TRUNC, 0666);  // creating same file at destination using open systemcall
            if (dest_fd == -1) {
                perror("Error occuring while opening destination file");
                close(src_fd);
                exit(EXIT_FAILURE);
            }

            while ((bytesRd = read(src_fd, buffer, sizeof(buffer))) > 0) // reading from source and writing the file at destination
            {
                bytesWr = write(dest_fd, buffer, bytesRd);
                if (bytesWr != bytesRd) {
                    perror("Error : bytesWr != bytesRd");
                    close(src_fd);
                    close(dest_fd);
                    exit(EXIT_FAILURE);
                }
            }

            close(src_fd); // closing source file descriptor
            close(dest_fd); // closing destination file descriptor

            printf("File copied to: %s\n", storage_dir); // Printing message when file copied
        }
        else if (strcmp(options, "-mv") == 0) // Logic for move
        {
            if (rename(file_loc_path, dest_path_loc) != 0) // Using rename to move file
            {
                perror("rename");
                exit(EXIT_FAILURE);
            }
            printf("File moved to: %s\n", storage_dir);
        } else
        {
            fprintf(stderr, "Invalid option: %s\n", options); // Handling invalid Option
            exit(EXIT_FAILURE);
        }
        srch_successful = 1;
        return 1; // To stop the traversal after 1st occurrence
    }
    return 0;
}
// Below function to implement Task 3
int copy_create_tar(const char *file_loc_path, const struct stat *struct_stat, int filetype, struct FTW *ftwbuf) {

        const char *file_extension = strrchr(file_loc_path, '.');

        // Check if the file extension of current file matches with given extension and it is at the end of the filename

        if (filetype == FTW_F && file_extension != NULL && strcmp(file_extension, extension) == 0) {
            // Copy the file to the storage directory
        match_found=1;
        char dest_path_loc[PATH_MAX];
        snprintf(dest_path_loc, PATH_MAX, "%s/temp/%s", storage_dir, file_loc_path + ftwbuf->base); // To make destination file path
        int src_fd, dest_fd;
        char buffer[4096];
        ssize_t bytesRd, bytesWr;
        // Logic for copying file into storage_dir to create tar.
        src_fd = open(file_loc_path, O_RDONLY);                              //For opening file at source
        if (src_fd == -1) {
            perror("Error occuring while opening source file");
            return 0;
        }

        dest_fd = open(dest_path_loc, O_WRONLY | O_CREAT | O_TRUNC, 0666);  // creating same file at destination using open systemcall
        if (dest_fd == -1) {
            perror("Error occuring while opening source file");
            close(src_fd);
            return 0;
        }

        while ((bytesRd = read(src_fd, buffer, sizeof(buffer))) > 0) // reading from source and writing the file at destination
        {
            bytesWr = write(dest_fd, buffer, bytesRd);
            if (bytesWr != bytesRd) {
                perror("Error : bytesWr != bytesRd");
                close(src_fd);
                close(dest_fd);
                return 0;
            }
        }

        close(src_fd);  // closing source file descriptor
        close(dest_fd); // closing destination file descriptor

        printf("File copied to: %s\n", dest_path_loc); // Printing message when files copied at destination storage_dir

        return 0;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // Program will only take 3, 4 and 5 arguments
    if (argc != 4 && argc != 5 && argc!= 3)
    {
        fprintf(stderr, "Usage: %s [root_dir] [filename] or %s [root_dir] [storage_dir] [options] [filename] or %s [root_dir] [storage_dir] [extension]\n", argv[0], argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    //  root_dir = argv[1];
    // To Check if given storage_dir is already an absolute path
    if (argv[1][0] != '/') {
         // If it is not, it will add the user's home directory at the starting
        const char *home_dir = getenv("HOME");
        if (home_dir == NULL) {
            fprintf(stderr, "Error: $HOME Kinldy Set HOME variable.\n");
            exit(EXIT_FAILURE);
        }

        root_dir = malloc(PATH_MAX);
        if (root_dir == NULL) {
            perror("Error (malloc): root_dir == NULL");
            exit(EXIT_FAILURE);
        }

        snprintf(root_dir, PATH_MAX, "%s/%s", home_dir, argv[1]);
    } else {
        // Use it as it is if it is absolute path
        root_dir = realpath(argv[1], NULL);
        if (root_dir == NULL) {
            perror("Error (realpath): Invalid Root directory");
            exit(EXIT_FAILURE);
        }
    }
    global_args_count = argc;

    if (argc == 4) {
    extension = argv[3];
    // storage_dir = argv[2];
    // To Check if given storage_dir is already an absolute path
    if (argv[2][0] != '/') {
     // If it is not, it will add the user's home directory at the starting
        const char *home_dir = getenv("HOME");
        if (home_dir == NULL) {
            fprintf(stderr, "Error: $HOME Kinldy Set HOME variable.\n");
            exit(EXIT_FAILURE);
        }

        storage_dir = malloc(PATH_MAX);
        if (storage_dir == NULL) {
            perror("Error (malloc): storage_dir == NULL");
            exit(EXIT_FAILURE);
        }

        snprintf(storage_dir, PATH_MAX, "%s/%s", home_dir, argv[2]);
    } else {
        // Use it as it is if it is absolute path
        storage_dir = realpath(argv[2], NULL);
        if (storage_dir == NULL) {
        //perror("Error (realpath): Invalid Storage Directory"); // will not make sense
            exit(EXIT_FAILURE);
        }
    }

    // Createing the storage_dir if it is not present!!
    if (mkdir(storage_dir, 0777) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    // Create a temp folder within the storage directory to make its tar!!
    char temp_dir[PATH_MAX];
    snprintf(temp_dir, PATH_MAX, "%s/temp", storage_dir);
    if (mkdir(temp_dir, 0777) == -1) {
        perror("mkdir temp");
        exit(EXIT_FAILURE);
    }

    // Copy files with the given extension to the temporary folder that we created inside storage_dir
    if (nftw(root_dir,copy_create_tar , 20, 0) == -1) {
        perror("nftw");
        exit(EXIT_FAILURE);
    }

    // Check if any files were copied and handling case where no file with matching extension found
    if (match_found == 0) // If files not copied then match_found == 0
    {
        printf("No files matching the extension found for copying.\n");
        return EXIT_SUCCESS; // Exit without creating tar file
    }

    //Here we are creating tar archive of the temp folder
    char tar_cmd[PATH_MAX];
    snprintf(tar_cmd, PATH_MAX, "tar -cvf %s/a1.tar -C %s/temp .", storage_dir, storage_dir);
    system(tar_cmd);

    }
    else
    {
            filename = argv[2];

            if (argc == 5)
            {
                // storage_dir = argv[2];
                // To Check if given storage_dir is already an absolute path
                if (argv[2][0] != '/')
                {
                    // If it is not, it will add the user's home directory at the starting
                    const char *home_dir = getenv("HOME");
                    if (home_dir == NULL)
                    {
                        fprintf(stderr, "Error: $HOME Kinldy Set HOME variable.\n");
                        exit(EXIT_FAILURE);
                    }

                    storage_dir = malloc(PATH_MAX);
                    if (storage_dir == NULL)
                    {
                    perror("Error (malloc): storage_dir == NULL");
                    exit(EXIT_FAILURE);
                    }

                    snprintf(storage_dir, PATH_MAX, "%s/%s", home_dir, argv[2]);
                }
            else {
                // Use it as it is if it is absolute path
                storage_dir = realpath(argv[2], NULL);
                if (storage_dir == NULL)
                    {
                        perror("Error (realpath): Invalid Storage Directory");
                        exit(EXIT_FAILURE);
                    }
                }
            options = argv[3];
            filename = argv[4];
            }

        if (nftw(root_dir, search_file, 20, FTW_PHYS) == -1)
        {
            perror("Error (nftw): Invaild root_dir");
            exit(EXIT_FAILURE);
        }

        if (!srch_successful)
        {
            printf("Search Unsuccessful: File not found in %s and its subdirectories.\n", root_dir);
        }
    }
    return 0;
}