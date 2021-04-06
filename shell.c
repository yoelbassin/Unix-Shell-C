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

int out_file, in_file = 0;

int in_restore, out_restore;

char *infile_path, *outfile_path;

void stringRemove(char *word, int idxToDel)
{
    memmove(&word[idxToDel], &word[idxToDel + 1], strlen(word) - idxToDel);
}

void tstring(char *word, int idxToDel)
{
    for (int i = 0; i < idxToDel; ++i)
        stringRemove(word, 0);
}

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
    out_file, in_file = 0;
    int i = 0;
    int infile_index, outfile_index = -1;
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
        printf("\n%s\n", token);
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
    args[i] = NULL;
    printf("\n%s\n", args[i]);
    return background;
} 

int parsePipe(char *command, char ***args)
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
                printf("\n%s\n", token);
        token = strtok(NULL, "|");
        i++;
    }

    for (int j = 0; j < i; ++j)
    {    printf("\nPipeline1\n");
        parseCommand(args1[j], args2);
         printf("\nPipeline1\n");
        args[j] = memcpy(args[j], args2, MAX_LINE);
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

void pipes(char *command)
{

    printf("\nPipeline\n");
    char **args[MAX_LINE];
    int n = parsePipe(command, args);
    printf("\nPipeline\n");
    int i;
    pid_t pid;
    int input, fd[2];
    input = 0;

    for (i = 0; i < n - 1; ++i)
    {
        printf("\nPipeline 2\n");
        pipe(fd);
        pid_t pid1;

        pid = fork();

        int output = fd[1];
        if (pid == 0)
        {
            if (input != 0)
            {
                dup2(input, STDIN_FILENO);
                close(input);
            }
            if (output != 0)
            {
                dup2(output, STDOUT_FILENO);
                close(output);
            }
            execc(args[i]);
        }
        close(fd[1]);
        input = fd[0];
    }
    if (input != 0)
    {
        dup2(input, 0);
    }
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

    out_file, in_file = 0;

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
