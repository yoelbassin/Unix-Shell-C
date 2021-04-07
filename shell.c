#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define HISTORY_PATH ".history"

int out_file = 0, in_file = 0;

int in_restore, out_restore;

int pipe_flag = 0;

char *infile_path, *outfile_path;

int parseCommand(char *command, char **args)
{
    int background = 0;
    if (command[strlen(command) - 2] == '&')
    {
        background = 1;
        command[strlen(command) - 2] = 0;
    }

    command[strlen(command) - 1] = 0;

    char *token = strtok(command, " ");
    out_file = 0, in_file = 0;
    int i = 0;
    int infile_index = -1, outfile_index = -1;

    while (token != NULL)
    {
        if (!strcmp(token, "<"))
        {
            in_file = 1;
            infile_index = i;
            token = strtok(NULL, " ");
            continue;
        }
        else if (!strcmp(token, ">"))
        {
            outfile_index = i;
            out_file = 1;
            token = strtok(NULL, " ");
            continue;
        }

        args[i] = token;
        token = strtok(NULL, " ");
        i++;
    }

    if (infile_index > -1)
    {
        infile_path = args[infile_index];
    }
    if (outfile_index > -1)
    {
        outfile_path = args[outfile_index];
    }

    if (pipe_flag == 0)
    {
        args[i] = NULL;
    }
    return background;
}

int parsePipe(char *command, char **args[])
{
    printf("\n%s\n", command);
    char *args1[MAX_LINE];
    char *args2[MAX_LINE];
    command[strlen(command) - 1] = 0;
    char *token = strtok(command, "|");
    int i = 0;
    while (token != NULL)
    {
        args1[i] = token;
        token = strtok(NULL, "|");
        i++;
    }

    for (int j = 0; j < i; ++j)
    {
        parseCommand(args1[j], args2);
        int k = 0;
        args[j] = malloc(sizeof(args2));
        while (args2[k] != NULL)
        {

            args[j][k] = strdup(args2[k]);
            k++;
        }

        memset(args2, 0, MAX_LINE);
    }
    return i;
}

void saveCommand(char **args)
{

    FILE *file = fopen(HISTORY_PATH, "a");
    int i = 0;
    while (args[i] != NULL)
    {
        fprintf(file, "%s ", args[i]);
        ++i;
    }
    fprintf(file, "\n");
    fclose(file);
}

void execc(char **args)
{
    //printf("\nExecuting %s\n", args[0]);
    execvp(args[0], args);
    printf("[!] command \'%s\' not found\n", args[0]);
    exit(0);
}

void useHistory(char **args, int command_count)
{
    FILE *file = fopen(HISTORY_PATH, "r");

    if (file == NULL)
    {
        printf("No history\n");
        exit(0);
    }

    char *line = NULL;
    int n = 10;
    size_t len = 0;

    if (args[1] != NULL)
        n = atoi(args[1]);
    if (!strcmp(args[0], "!!"))
        n = command_count - 1;

    if (!strcmp(args[0], "hist"))
    {
        int j = 0;
        printf("command count: %d\n", command_count);
        while ((getline(&line, &len, file)) != -1)
        {
            if (j >= command_count - n)
                printf("%s", line);
            j++;
        }
        printf("\n");
        fclose(file);
        exit(0);
    }
    else
    {
        for (int i = 0; i <= n; ++i)
        {
            if (getline(&line, &len, file) == -1)
            {
                printf("[!] last command number is %d\n", i - 1);
                exit(0);
            }
        }
        printf("Command number %d: %s", n, line);
        parseCommand(line, args);
        execc(args);
        exit(0);
    }
}

void pipeSend(int command_count, char **args, int *fd)
{
    char buffer[100];
    snprintf(buffer, 100, "%d", command_count);
    close(fd[0]);
    write(fd[1], buffer, strlen(buffer) + 1);
}

void redirection()
{
    if (out_file == 1)
    {
        out_file = 0;
        int output = open(outfile_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (output == -1)
        {
            perror("Output File Failed");
            exit(0);
        }
        out_restore = dup(STDOUT_FILENO);
        dup2(output, STDOUT_FILENO);
    }
    if (in_file == 1)
    {
        in_file = 0;
        int input = open(infile_path, O_RDONLY);
        if (input == -1)
        {
            perror("Output File Failed");
            exit(0);
        }
        in_restore = dup(STDIN_FILENO);
        dup2(input, STDIN_FILENO);
    }
}

void spawn_proc(int in, int out, char **args)
{
    pid_t pid;

    if ((pid = fork()) == 0)
    {
        if (in != 0)
        {
            dup2(in, 0);
            close(in);
        }

        if (out != 1)
        {
            dup2(out, 1);
            close(out);
        }

        execc(args);
    }
}

void pipes(char *command)
{
    strcat(command, "\n");
    pipe_flag = 1;
    char **args[MAX_LINE];
    int n = parsePipe(command, args);


    for (int j = 0; j < n; j++)
    {
        printf("{");
        for (int k = 0; k < 3; k++)
        {
            printf("%s, ", args[j][k]);
        }
        printf("}\n");
    }
    
    int i;
    pid_t pid;
    int in, fd[2];

    /* The first process should get its input from the original file descriptor 0.  */
    in = 0;

    /* Note the loop bound, we spawn here all, but the last stage of the pipeline.  */
    for (i = 0; i < n - 1; ++i)
    {
        pipe(fd);

        /* f [1] is the write end of the pipe, we carry `in` from the prev iteration.  */
        spawn_proc(in, fd[1], args[i]);

        /* No need for the write end of the pipe, the child will write here.  */
        close(fd[1]);

        /* Keep the read end of the pipe, the next child will read from there.  */
        in = fd[0];
    }

    /* Last stage of the pipeline - set stdin be the read end of the previous pipe
     and output to the original file descriptor 1. */
    if (in != 0)
        dup2(in, 0);

    /* Execute the last stage with the current process. */
    execc(args[i]);
}

int checkPipe(char *command)
{
    if (strchr(command, '|') != NULL)
        return 1;
    return 0;
}

int main(void)
{
    char *args[MAX_LINE / 2 + 1];

    out_file = 0, in_file = 0;

    int running = 1;

    int command_count = 0;

    int background = 0;

    int fd[2];

    if (pipe(fd) == -1)
    {
        fprintf(stderr, "Pipe Failed");
        return 1;
    }

    while (running)
    {

        out_restore = dup(STDOUT_FILENO);
        in_restore = dup(STDIN_FILENO);

        printf("osh> ");
        fflush(stdout);
        background = 0;

        char command[MAX_LINE + 1];
        fgets(command, MAX_LINE + 1, stdin);

        if (!strcmp(command, "e\n") || strlen(command) <= 0)
        {
            running = 0;
            continue;
        }

        if (!strcmp(command, "\n"))
        {
            continue;
        }

        if (checkPipe(command))
        {
            int pid_ = fork();
            if (pid_ == 0)
                pipes(command);
            else
            {
                wait(NULL);
                pipe_flag = 0;
                continue;
            }
        }
        background = parseCommand(command, args);

        if (out_file == 1 || in_file == 1)
        {
            redirection();
        }

        int pid = fork();

        if (pid < 0)
        {
            perror("Fork Failed");
            return 1;
        }

        if (pid == 0)
        {
            if (!strcmp(args[0], "!") || !strcmp(args[0], "hist") || !strcmp(args[0], "!!"))
            {
                pipeSend(command_count, args, fd);
                useHistory(args, command_count);
            }
            command_count++;
            pipeSend(command_count, args, fd);
            saveCommand(args);
            execc(args);
        }

        else
        {
            char input_str[100];
            read(fd[0], input_str, 100);

            command_count = atoi(input_str);
            if (!background)
                wait(NULL);
        }
        dup2(in_restore, STDIN_FILENO);
        dup2(out_restore, STDOUT_FILENO);
    }
    remove(HISTORY_PATH);
    close(fd[1]);
    close(fd[0]);

    return 0;
}