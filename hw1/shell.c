#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <sys/signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

#define BUFSIZE 1024

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

/* Array to keep track of background pids */
int background_pids[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* Current index of background pids */
int pid_index = 0;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "show current working directory"},
  {cmd_cd, "cd", "change working directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Prints the working Directory */
int cmd_pwd(unused struct tokens *tokens) {
  char buf[BUFSIZE];
  char* pwd;
  pwd = getcwd(buf, BUFSIZE);
  printf("%s\n", pwd);
  return 0;
}

/* Changes directory for given directory path */
int cmd_cd(struct tokens *tokens) {
  int success;
  if ((tokens_get_length(tokens)) == 2) {
      char* path;
      path = tokens_get_token(tokens, 1);
      success = chdir(path);
      if (success == -1) {
        printf("-bash: cd: %s: No such file or directory\n", path);
        return -1;
      }
  }
  return 0;
}

/* Waits until all background jobs have terminated before returning to the prompt. */
  int cmd_wait(unused struct tokens *tokens) {
    int status;
    for (int i = 0; i < 10; i++) {
      if (background_pids[i] != 0) {
        waitpid(background_pids[i], &status, 0);
      }
    }
    return 0;
  }


/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);
    int token_length = tokens_get_length(tokens);
    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));
    int status;

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
      tcsetpgrp(0, getpgrp());
    } else {
      int i;
      pid_t cpid;
      signal(SIGTTOU, SIG_IGN);
      cpid = fork();
      pid_t subpid = getpid();
      setpgid(subpid, subpid);
      int background_p = 0;
      if (strcmp(tokens_get_token(tokens, token_length - 1), "&") == 0) {
        background_p = 1;
      }
      if (background_p == 0) {
        tcsetpgrp(0, getpgrp());
      }
      const char* arg0 = tokens_get_token(tokens, 0);
      if (cpid > 0){
        waitpid(cpid, &status, WNOHANG|WUNTRACED);
        tcsetpgrp(0, shell_pgid);
      } else if (cpid == 0) {
        if (strcmp(tokens_get_token(tokens, token_length - 1), "&") == 0) {
          background_pids[pid_index] = getpid();
          pid_index++;
        }
        char* args[(token_length+1)];
        int redir_out_to_file = 0;
        int redir_file_to_out = 0;
        for (i = 0; i < token_length; i++) {
          args[i] = tokens_get_token(tokens, i);
          if ((strcmp(args[i], ">")) == 0) {
            redir_file_to_out = 1;
            args[i] = NULL;
            break;
          }
          if ((strcmp(args[i], "<")) == 0) {
            redir_out_to_file = 1;
            args[i] = NULL;
            break;
          }
        }
        int newfd;
        if (redir_file_to_out || redir_out_to_file) {
          char* filename = tokens_get_token(tokens, (token_length -1));
          if (background_p == 1) {
            filename = tokens_get_token(tokens, (token_length -2));
          }
          newfd = open(filename, O_CREAT|O_RDWR, 0644);
          if(redir_file_to_out) {
            dup2(newfd, 1);
          }
          if(redir_out_to_file) {
            dup2(newfd, 0);
          }
        }
        args[token_length] = (char *) NULL;
        if (arg0[0] != '/') {
          const char *tok;
          char *dup;
          dup = getenv("PATH");
          const char colon[] = ":";
          while ((tok = strsep(&dup, colon))) {
            char path_f[strlen(tok) + 2 + strlen(arg0)];
            strcpy(path_f, tok);
            strcat(path_f, "/");
            strcat(path_f, (arg0));
            path_f[(strlen(tok) + strlen(arg0)) + 1] = '\0';
            if (access(path_f, F_OK) == 0) {
              args[0] = path_f;
              execv(((const char*) path_f), args);
              if (redir_file_to_out || redir_out_to_file) {
                close(newfd);
              }
            }
          } 
        } else {
          execv(arg0, args);
          if (redir_file_to_out || redir_out_to_file) {
            close(newfd);
          }
        }
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }
  return 0;
}
