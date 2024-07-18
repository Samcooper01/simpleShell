#include "./cshell_lib.h"

#define MAX_USER_LINE   1024
#define MAX_COMMANDS    100
#define MAX_ARGS 100

char* user_line;
char** user_args;
int user_exit = 1;

char* last_directory;
char* saved_directories_filepath;

FILE *command_hist;
char * command_hist_filepath;

void get_next_line(){
    //Allocate space for next line and arguments
    user_args = (char **)malloc(128 * sizeof(char*));
    if(user_args == NULL) {
        perror("user_args malloc failed");
        exit(1);
    }
    for (int i = 0; i < 128; i++) {
        user_args[i] = NULL;
    }
    user_line = (char *)malloc(MAX_USER_LINE * sizeof(char));
    if(user_line == NULL) {
        perror("user_line malloc failed");
        exit(1);
    }

    //Get next line
    fgets(user_line, MAX_USER_LINE, stdin);

    //Log line to saved_directories
    fprintf(command_hist, "%s", user_line);
    fclose(command_hist);
    command_hist = fopen(command_hist_filepath, "a");
    if(command_hist == NULL) {
        printf("FAILED TO LOAD .command_history file");
        return;
    }
}

/*

###### Built In commands ######

*/
void update_last_dir() {
    last_directory = getcwd(last_directory, 1024);
}

void chdir_back(int num_times) {
    for(int i = 0; i < num_times; i++) {
        chdir("..");
    }
}

void chdir_last() {
    chdir(last_directory);
}

int arg_in_saved_dir(char * arg) {
    FILE *saved_dir_file = fopen(saved_directories_filepath, "r");
    if(saved_dir_file == NULL) {
        printf("ERROR: .saved_directories file not found\n");
        fclose(saved_dir_file);
        return 0;
    }
    char line_buffer[1024];
    while(fgets(line_buffer, sizeof(line_buffer), saved_dir_file) != NULL) {
        char *token = strtok(line_buffer, " ");
        if(strcmp(token, arg) == 0) {
            token = strtok(NULL, " ");

            char *newline_ptr = strchr(token, '\n');

            if(newline_ptr != NULL) {
                *newline_ptr = '\0';
            }

            chdir(token);
            fclose(saved_dir_file);
            return 1;
        }
    }
    fclose(saved_dir_file);
    return 0;
}

void save_dir() {
    FILE *saved_dir_file = fopen(saved_directories_filepath, "a");
    if(saved_dir_file == NULL) {
        printf("ERROR: .saved_directories file not found\n");
        fclose(saved_dir_file);
        return;
    }

    if(user_args[1] == NULL && user_args[2] == NULL) {
        printf("USAGE ERROR");
    }

    fprintf(saved_dir_file, "%s %s\n", user_args[1], user_args[2]);

    printf("Directory: %s successfully saved to variable '%s'\n", user_args[2], user_args[1]);
    fclose(saved_dir_file);
}

void list_dirs() {
    FILE *saved_dir_file = fopen(saved_directories_filepath, "r");
    if(saved_dir_file == NULL) {
        printf("ERROR");
        return;
    }
    printf("Printing saved directories found in .saved_directories:\n\n");
    char line_buffer[1024];
    int index = 0;
    while(fgets(line_buffer, sizeof(line_buffer), saved_dir_file) != NULL) {
        index++;
        printf("%d: %s", index, line_buffer);
    }
    printf("\n");
    fclose(saved_dir_file);
}

void display_history() {
    char display_buffer[1024];
    fclose(command_hist);
    command_hist = fopen(command_hist_filepath, "r");
    if(command_hist == NULL) {
        printf("FAILED TO LOAD .command_history file");
        return;
    }
    printf("\nCommand history:\n\n");
    while(fgets(display_buffer, sizeof(display_buffer), command_hist) != NULL) {
        printf("%s", display_buffer);
    }
    fclose(command_hist);
    command_hist = fopen(command_hist_filepath, "a");
    if(command_hist == NULL) {
        printf("FAILED TO LOAD .command_history file");
        return;
    }
    printf("\n");
}

int builtin_commands(char *command) {
    //Check for user input exit
    if(strcasecmp(command, "exit") == 0) {
        user_exit = 0;
        return 1;
    }

    if(strcmp(command, "cd") == 0) {
        if(user_args[1] == NULL) {
            printf("ERROR: expected argument to cd\n");
            return 1;
        }
        if(arg_in_saved_dir(user_args[1])) {
            return 1;
        }
        update_last_dir();
        chdir(user_args[1]);
        return 1;
    }

    if(strcmp(command, "cd..") == 0) {
        if(user_args[1] == NULL) {
            printf("USAGE ERROR");
            return 1;
        }
        update_last_dir();
        chdir_back(atoi(user_args[1]));
        return 1;
    }

    if(strcmp(command, "cd>>") == 0) {
        chdir_last();
        return 1;
    }

    if(strcmp(command, "savedir") == 0) {
        save_dir();
        return 1;
    }

    if(strcmp(command, "lsdirs") == 0) {
        list_dirs();
        return 1;
    }

    if(strcmp(command, "history") == 0) {
        display_history();
        return 1;
    }
    return 0;
}

/*

###### End of Built In commands ######

*/

void execute_args() {
    char *** commands_to_run = (char ***)malloc(MAX_COMMANDS * sizeof(char **));

    //allocate commands_to_run
    for (int i = 0; i < MAX_COMMANDS; i++) {
        commands_to_run[i] = (char **)malloc(MAX_ARGS * sizeof(char *));
        if (commands_to_run[i] == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        // Initialize the arguments pointers to NULL
        for (int j = 0; j < MAX_ARGS; j++) {
            commands_to_run[i][j] = NULL;
        }
    }

    //Get args for each pipe section
    //If no pipe then this will just get the args into the first row
    int command_index = 0;
    int j = 0;
    for(int i = 0; i < sizeof(user_args); i++) {
        if(user_args[i] == NULL) {
            break;
        }
        if(strcmp(user_args[i], "|") == 0) {
            command_index++;
            j = 0;
            continue;
        }
        commands_to_run[command_index][j] = user_args[i];
        j++;
    }
    command_index++;

    //Pipeline allocate
    pid_t pid;
    int i = 0;
    int in = 0;
    int fd [2];
    int builtin_status = 0;

    //Execute loop. this only runs once if no pipelines
    for(i = 0; i < command_index - 1; ++i)
    {

        pipe (fd);

        int out = fd[1];


        pid = fork();

        if (pid < 0) {
            // Fork failed
            perror("fork");
        } else if (pid == 0) {
            // Child process (Spawned program from fork())
            //Not the last command
            if (in != 0)
            {
            dup2 (in, 0);
            close (in);
            }

            if (out != 1)
            {
            dup2 (out, 1);
            close (out);
            }

            builtin_status = builtin_commands(commands_to_run[i][0]);
            //If not a builtin_command then should be a linux command
            if(builtin_status != 1) {
                printf("command: %s\n", commands_to_run[i][0]);
                execvp (commands_to_run[i][0], (char * const *)commands_to_run[i]);
            }
        }
        else {
            int status;
            waitpid(pid, &status, WUNTRACED);
        }

        close (fd [1]);

        /* Keep the read end of the pipe, the next child will read from there.  */
        in = fd [0];

    }

    if (in != 0)
        dup2 (in, 0);

    builtin_status = builtin_commands(commands_to_run[i][0]);
    //This command goes to stdout
    if(builtin_status == 0) {
        int status = execvp (commands_to_run[i][0], (char * const *)commands_to_run[i]);
        printf("STATUS: %d", status);
    }
    close(fd[0]);
    close(fd[1]);

    //Free commands_to_run before next line
    for (int i = 0; i < MAX_COMMANDS; i++) {
        for (int j = 0; j < MAX_ARGS; j++) {
            if (commands_to_run[i][j] != NULL) {
                commands_to_run[i][j] = NULL;
            }
        }
        free(commands_to_run[i]);
    }
    free(commands_to_run);
}


void remove_newlines() {
    int position = 0;
    while(user_args[position] != NULL) {
        char *current_string = user_args[position];
        size_t len = strlen(current_string);
        if (len > 0 && current_string[len - 1] == '\n') {
            current_string[len - 1] = '\0';
        }
        position++;
    }
}

void parse_line_args() {
    char *arg = strtok(user_line, ARG_DELIM);
    int index = 0;
    user_args[index] = arg;

    while(arg != NULL) {
        index++;
        arg = strtok(NULL, ARG_DELIM);
        user_args[index] = arg;
    }
    index++;
    user_args[index] = NULL;
    //We just remove all newlines from the line
    remove_newlines(user_args);
}


void command_prompt() {
    printf("%s", SHELL_PROMPT_TOKEN);
    get_next_line();
}

void cleanup() {
    //No need to free user_args individually because user_args[i]
    //points to parts of user_line
    free(user_args);
    free(user_line);
}

void cshell_loop() {
    //Shell state machine
    do {
        command_prompt();
        parse_line_args();
        execute_args();
        cleanup();
    } while(user_exit);
}

void init() {

    update_last_dir();
    saved_directories_filepath = getcwd(saved_directories_filepath, 1024);
    strcat(saved_directories_filepath, "/.saved_directories");

    command_hist_filepath = getcwd(command_hist_filepath, 1024);
    strcat(command_hist_filepath, "/.command_history");

    command_hist = fopen(command_hist_filepath, "w");
    if(command_hist == NULL) {
        printf("FAILED TO LOAD .command_history file");
        return;
    }
}

int main() {

    init();

    //Main application Loop
    cshell_loop();

}
