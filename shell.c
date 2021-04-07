#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LINE 1024
#define HISTORY_PATH ".history"

char dir[MAX_LINE];

int out_file = 0, in_file = 0;

int in_restore, out_restore;

int pipe_flag = 0;

char *infile_path, *outfile_path;

int running = 1;

int in_process = 0;

void remIndex(char *word, int idxToDel)
{
    memmove(&word[idxToDel], &word[idxToDel + 1], strlen(word) - idxToDel);
}

int parseCommand(char *command, char **args)
{
    int background = 0;

    int string_flag = 0;

    if (command[strlen(command) - 2] == '&')
    {
        background = 1;
        command[strlen(command) - 2] = 0;
    }

    if (command[strlen(command) - 1] == '\n')
        command[strlen(command) - 1] = 0;

    char *token = strtok(command, " ");
    out_file = 0, in_file = 0;
    int i = 0;
    int infile_index = -1, outfile_index = -1;

    while (token != NULL)
    {

        if (token[0] == '\'')
        {
            string_flag = 1;
        }

        if (string_flag == 1)
        {
            if (token[0] == '\'')
            {
                string_flag = 1;
                args[i] = token;
                remIndex(args[i], 0);
            }
            else
            {
                strncat(args[i], " ", 2);
                strncat(args[i], token, strlen(token));
            }
            if (token[strlen(token) - 1] == '\'')
            {
                remIndex(args[i], strlen(args[i]) - 1);
                string_flag = 0;
                i++;
            }
            token = strtok(NULL, " ");
            continue;
        }

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

int parsePipe(char *command, char *args[])
{
    command[strlen(command) - 1] = 0;
    char *token = strtok(command, "|");
    int i = 0;
    while (token != NULL)
    {
        args[i] = token;
        token = strtok(NULL, "|");
        i++;
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
    int n = command_count;
    size_t len = 0;

    if (args[1] != NULL)
        n = atoi(args[1]);
    if (!strcmp(args[0], "!!"))
        n = command_count - 1;

    if (!strcmp(args[0], "history"))
    {
        int j = 0;
        printf("command count: %d\n", command_count);
        while ((getline(&line, &len, file)) != -1)
        {
            if (j >= command_count - n)
                printf("%d %s", j, line);
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
    strcat(command, "\n");
    pipe_flag = 1;
    char *commands[MAX_LINE];
    char *args[MAX_LINE];
    int n = parsePipe(command, commands);

    int i;
    pid_t pid;
    int in = 0, fd[2];

    /* Note the loop bound, we spawn here all, but the last stage of the pipeline.  */
    for (i = 0; i < n - 1; ++i)
    {
        parseCommand(commands[i], args);

        pipe(fd);

        /* f [1] is the write end of the pipe, we carry `in` from the prev iteration.  */
        pid_t pid;

        int out = fd[1];

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

        /* No need for the write end of the pipe, the child will write here.  */
        close(fd[1]);

        /* Keep the read end of the pipe, the next child will read from there.  */
        in = fd[0];

        memset(args, 0, MAX_LINE);
    }

    parseCommand(commands[i], args);

    /* Last stage of the pipeline - set stdin be the read end of the previous pipe
     and output to the original file descriptor 1. */
    if (in != 0)
        dup2(in, 0);

    /* Execute the last stage with the current process. */
    execc(args);
}

int checkPipe(char *command)
{
    if (strchr(command, '|') != NULL)
        return 1;
    return 0;
}

void signalHandler(int signo)
{
    write(1, "\n", 1);
    if (in_process == 0)
        printf("%s>", dir);
    fflush(stdout);
}

int main(void)
{

    int pid0 = fork();

    if (pid0 == 0)
    {
        char *clear[] = {"clear", NULL};
        execc(clear);
    }

    wait(NULL);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGTSTP, signalHandler);

    char *args[MAX_LINE / 2 + 1];

    out_file = 0, in_file = 0;

    int command_count = 0;

    int background = 0;

    rl_bind_key('\t', rl_complete);

    int fd[2];
    char *command;
    if (pipe(fd) == -1)
    {
        fprintf(stderr, "Pipe Failed");
        return 1;
    }

    while (running)
    {

        in_process = 0;

        getcwd(dir, MAX_LINE);

        out_restore = dup(STDOUT_FILENO);
        in_restore = dup(STDIN_FILENO);

        printf("%s>", dir);
        fflush(stdout);
        background = 0;

        command = readline(" ");
        in_process = 1;
        if (!command[0])
        {
            free(command);
            continue;
        }
        if (!strcmp(command, "\n"))
        {
            continue;
        }

        if (!strcmp(command, "exit") || !strcmp(command, "quit"))
        {
            running = 0;
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

        if (!strcmp(args[0], "cd"))
        {
            chdir(args[1]);
            continue;
        }

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
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            if (!strcmp(args[0], "!") || !strcmp(args[0], "history") || !strcmp(args[0], "!!"))
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
