/**************************************************************
 * SMALLSH
 * Description: File for a small shell program
 * How to compile: Compile using: gcc -std=gnu99 -Wall -Wextra -g -o smallsh smallsh.c
 * How to run: After compililng into an executable named smallsh run using './smallsh'
 * ************************************************************/

//Including everything in the kitchen sink
#include <sys/stat.h> //defines fstat, lstat, stat
#include <stdio.h> //fprint, scanf, printf, perror, fflush, size_t, FILE, input/output functions
#include <sys/types.h> //pid_t
#include <unistd.h> //getpid, getppid, fork, execv
#include <stdlib.h> //malloc, free, abort, exit, memory allocatin/free functions
#include <sys/wait.h> //wait, waitpid
#include <signal.h> //sigaction, sigaction struc, 
#include <string.h> // size_t, char memchr, strcncmp
#include <errno.h> //error handling, perror
#include <ctype.h> //isspace
#include <unistd.h> //dup2
#include <fcntl.h> //open()
#include <fcntl.h> //fcntl

//Define constants
#define USER_INPUT_SIZE 2048
#define ARGUMENT_SIZE 512
#define MAX_PATH 260
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

//Global variable for toggling foreground only mode on SIGTSTP signal
int global_signal_toggle = 0;

/*Function declarations*/
void command_prompt(char* outputArray[], int pid, char inputFile[], char outputFile[]); //Function to perform command prompt and take in user input
void change_directory(char pathname[]); //Function to perform cd and change directory
int execute_command(char* argArray[], int background, int *exitMethod, char inputReDir[], char outputReDir[]); //Function to execute all other commands using fork(), exec(), waitpid()
void print_status(int exitVal, int exitMethod); //Function to print out status message for exit value and method
void handle_SIGTSTP(int signo); //Function to handle ctrl+C signal interruption

/**************************************************************
 * Main function of smallsh
 * Description: This is the main function of smallsh that will run on start of smallsh until exit.
 * Handles signals, calls command_prompt, change_directory, execute_command, print_status, handle_SIGTSTP
 * INPUT: None
 * OUTPUT: None
 * ************************************************************/
int main () {
  char *inputArr[ARGUMENT_SIZE]; //stores user input for executing commands as tokens
  char input_file[USER_INPUT_SIZE]; //for input redirection
  char output_file[USER_INPUT_SIZE]; //for output redirection
  int processID = getpid();
  int background = 0; //bool to check if process runs in background
  
  //Variables to help with handling children
  int childExitMethod = 0; //Exit type for child process, 0 for exit, 1 for terminated by signal
  int *childExitAddress;
  childExitAddress = &childExitMethod;
  int childExitVal = 0; //Holds return value from execute_command
  int childStatus;
  pid_t deadChildren;

  struct sigaction ignore_sigint = {0}, toggle_sigtstp = {0}; //handle signals SIGINT and SIGTSTP

  //ignore SIGINT for shell
  ignore_sigint.sa_handler = SIG_IGN;
  ignore_sigint.sa_flags = 0;
  sigaction(SIGINT, &ignore_sigint, NULL);

  //Handle SIGTSTP by toggling background boolean; 
  toggle_sigtstp.sa_handler = handle_SIGTSTP;
  sigfillset(&toggle_sigtstp.sa_mask);
  toggle_sigtstp.sa_flags = SA_RESTART;
  sigaction(SIGTSTP, &toggle_sigtstp, NULL);
  

  //printf("Hello SMALLSH! \n");
  /*This is the main loop for smallsh that prompts user and attempts to execute their command*/
  for (;;) {
    fflush(NULL);

    /*Clear all variables before next command prompt */
    for (int i = 0; inputArr[i] != NULL; i++) { //clear array storing user input as tokens
      inputArr[i] = NULL;    
    }
    background = 0; //clear check for if command is to be run in background
    memset(&input_file[0], 0, sizeof(input_file)); //clear char array for input
    memset(&output_file[0], 0, sizeof(output_file)); //clear char array for output

    /* This while loop checks for dead children to clean up and also print their exit status 
     * Citation: tutorialspoint.com/unix_system_calls/waitpid.htm */
    while (((deadChildren = waitpid(-1, &childStatus, WNOHANG)) > 0)) {
        printf("background pid %d is done: ", deadChildren);
        fflush(stdout);
        if (WIFEXITED(childStatus)) { //if child process exited normally
            childExitVal = WEXITSTATUS(childStatus);
            childExitMethod = 0;
          }
        else { //if child exited via signal
            childExitVal = WTERMSIG(childStatus);
            childExitMethod = 1;
          }
        print_status(childExitVal,childExitMethod); //append exit status 
      } //end while loop 

    /*Take in user input */
    command_prompt(inputArr, processID, input_file, output_file); 

    /*Handle blank lines and comments*/
    if (inputArr[0] == NULL) {
      //printf("Blank line or comment detected \n");
      continue;
    }

    /*Handle background '&' */
    int backcheck = 0;
    while (inputArr[backcheck+1] != NULL) { //loops to end of inputArr to see if last command is &
      backcheck++;
    }
    if (inputArr[backcheck][0] == '&' && strlen(inputArr[backcheck]) == 1) {
      if (global_signal_toggle == 0) { //If & is passed and not running in foreground mode
        background = 1;
      }
      inputArr[backcheck] = NULL; //clear & from inputArr 
      //printf("Setting process to background: %i \n", background);
    }//end background check

       
    /*Handle built-in exit command*/
    if (strcmp(inputArr[0], "exit") == 0) {
       break;
    } //end exit command

    /*Handle built-in cd command*/
    else if (strcmp(inputArr[0], "cd") == 0) {
      char currentwd[MAX_PATH];
      //printf("CD command executing...\nPATH is: %s\nHome is: %s\nROOT is: %s\n", getenv("PATH"), getenv("HOME"),getenv("ROOT"));
      //printf("Current directory now: %s\n", getcwd(currentwd, MAX_PATH));
      if (inputArr[1] == NULL) { //if no second argument is passed to cd default to HOME directory
        getcwd(currentwd, MAX_PATH);
        change_directory(getenv("HOME"));
      }
      else { //else change directory to second argument
        change_directory(inputArr[1]);
      }
    }//end cd command 
    
    /*Handle built in status command */
    else if (strcmp(inputArr[0], "status") == 0) {
      print_status(childExitVal, childExitMethod); 
    }//end status command
    
    /*Handle all other commands with execute_command, capture exit value in childExitVal */
    else {
      childExitVal = execute_command(inputArr, background, childExitAddress, input_file, output_file);
    }//end other command

    //Make sure all variables used from current loop are cleared out of paranoia
    for (int i = 0; inputArr[i] != NULL; i++) { //clear array storing user input as tokens
      inputArr[i] = NULL;    
    }
    background = 0; //clear check for if command is to be run in background
    memset(&input_file[0], 0, sizeof(input_file)); //clear char array for input
    memset(&output_file[0], 0, sizeof(output_file)); //clear char array for output
    
  } //end main if loop 

  //printf("Exiting smallsh: %i \n", processID);
  exit(0); //Exit smallsh 

  return 0;
} //end main func


/**************************************************************
 * command_prompt
 * Description: Handle user input from command prompt, prints : for each prompt line 
 * Syntax for passed commands should be: command [arg 1 arg2...] [< input_file] [> output_file] [&]
 * INPUT: outputArray - array for arguments from user input
 *        pid - the current process pid
 *        inputFile - array for storing input redirection path
 *        outputFile - array for storing output redirection path
 * OUTPUT: None
 * ************************************************************/


void command_prompt(char* outputArray[], int pid, char inputFile[], char outputFile[]) {
  char buffer[USER_INPUT_SIZE + 100]; //add 100 as extra buffer for variable expansion
  const char comment = '#';

  //variable expansion set up
  char var_expanse[] = "$$";
  char string_pid[100]; // store pid as string
  sprintf(string_pid, "%i", pid);
  int pid_len = strlen(string_pid);
  char *post_var_string; //pointer for strstr 
  //printf("PID is: %s \n", string_pid);


  /*Get user input*/
  fflush(stdout);
  printf(": ");
  fflush(stdout);
  fgets(buffer, USER_INPUT_SIZE, stdin);
  //fflush(NULL);
  //printf("Your input: %s \n", buffer);

  /*Check what length of string is in buffer */
  int string_length = strlen(buffer);
  //printf("Buffer length: %i \n", string_length);
  
  //Change newline to implicit null terminator \n -> \0
  for (int i = 0; i < string_length; i++) {
    if (buffer[i] == '\n') {
      //printf("Newline found %i \n", i);
      buffer[i] = '\0';
    }
  }

  /* Handle variable expansion of $$ in if else block *
   * Citation: Code for replacing a substring: 
   youtube.com/watch?v=0SU0nxlZiE*/
  while (strstr(buffer, var_expanse) != NULL) {
    post_var_string = strstr(buffer, var_expanse);
    memmove(post_var_string + pid_len, post_var_string + 2, strlen(post_var_string) - 1);
    memcpy(post_var_string, string_pid, pid_len);
  }
  //printf("Variable expanse of string: %s\n", buffer);
  
  
  /* Parse user input from buffer and save it into a struct, capturing the command and list of argumments.
   * Tokenize string using strtok */
  const char delim_space[2] = " ";
  char *token;
  int i = 0;
  
  token = strtok(buffer, delim_space);
  if (token == NULL) { //handle if input is blank
      //printf("Blank line found \n");
      return;
  }

  /*Main loop to tokenize user input string as arguments into outputArray */
  while (token != NULL) {
    string_length = strlen(token);
    //printf( "Token is: %s and length %i \n", token, string_length);
    
    /*Handle comment lines beginning with # */
    if (token[0] == comment) { 
      //printf("Comment line found \n");
      return;
    }

    //Check for input file with <
    if (token[0] == '<' && strlen(token) == 1) {
        token = strtok(NULL, delim_space); //advance token to next argument for input file path
        //printf("Setting input file to: %s\n", token);
        strcpy(inputFile, token);
        token = strtok(NULL, delim_space); //skip adding input file to outputArr
        if (token == NULL) {
          break;
        }
    }
    
    //Check for output file with >
    if (token[0] == '>' && strlen(token) == 1) {
        token = strtok(NULL, delim_space); //advance token to next argument for output file path
        //printf("Setting output file to: %s\n", token);
        strcpy(outputFile, token);
        token = strtok(NULL, delim_space); //skip adding output file to outputArr
        if (token == NULL) {
          break;
        }
    }

    outputArray[i] = strdup(token); //store tokens in outpurArray
    string_length = strlen(outputArray[i]);

    //printf("Array %i is %s and length %i \n", i, token, string_length);
    i++;
    token = strtok(NULL, delim_space); //advance token 
  }
}//end command prompt function




/**************************************************************
 * change_directory
 * Description: Changes the current working directory to the directory passed to it 
 * INPUT: pathname - string array of path to change current directory to
 * OUTPUT: None
 * ************************************************************/
void change_directory(char pathname[]) {
  //char cwd[MAX_PATH];
  //printf("Changing path to: %s\n", pathname);
  chdir(pathname);
  //printf("New directory now: %s\n", getcwd(cwd, MAX_PATH));
} //end change_directory function


/**************************************************************
 * execute_command
 * Description: General function that will handle any-non-built in functions
 * Will fork a child process to execute the command and run in either
 * foreground or background
 * INPUT: argArray - array of arguments to be passed to execvp()
 *        background - integer bool for if to run process in background or foreground
 *        exitMethod - address of integer representing exit/termination signal 
 *        inputRedir - string array for path for input redirection
 *        outputReDir - string array for path to output redirection
 * OUTPUT: Returns integer representing the exit value of the completed execvp() child process
 * ************************************************************/

int execute_command(char *argArray[], int background, int *exitMethod, char inputReDir[], char outputReDir[]) {
  //printf("Executing non-built in command, run in background: %i \n", background);

  int childStatus;
  int childPid;
  int exitStatus = 0;
  int inputSource;
  int outputTarget;
  struct sigaction SIGINT_action = {0}, ignore_sigtstp = {0}; //set up signal structures for handling SIGINT and SIGTSTP

  pid_t spawnpid = fork(); //Fork a new process, if successful spawnpid = 0
                           
  switch(spawnpid) { 

    case -1: //fork failed?!
      perror("fork() failed! \n");
      fflush(stdout);
      exit(1);
      break;
    
    case 0: //0 for spawnpid is the child process
      
        /* Handle SIGINT signal interupt 'ctrl + c' depending on foreground/background */
        if (background == 0) { //if process in foreground, treat sigint normally
          SIGINT_action.sa_handler = SIG_DFL;
        }
        else { //if process in background, ignore sigint
          SIGINT_action.sa_handler = SIG_IGN; 
        }
        SIGINT_action.sa_flags = 0;
        sigaction(SIGINT, &SIGINT_action, NULL);
        
        /* Handle SIGTSTP signal stop 'ctrl + z' by ignoring */
        ignore_sigtstp.sa_handler = SIG_IGN;
        ignore_sigtstp.sa_flags = 0;
        sigaction(SIGTSTP, &ignore_sigtstp, NULL);
        
        //printf("I am the child. My pid = %d\n", getpid());

        /* Handle input redirection*/
        if (strlen(inputReDir) != 0) { //check for input redirect
          //printf("Input redirected to: %s\n", inputReDir);
          inputSource = open(inputReDir, O_RDONLY); //open file in readonly
          if (inputSource == -1) { //if error opening print error and set exit to 1
            perror("source open()");
            fflush(stdout);
            exit(1);
          }
          else { 
            //printf("inputSource == %d\n", inputSource);
            int result = dup2(inputSource, 0);
            if (result == -1) { //if error redirecing stdin to input source
              perror("source dup2()");
              fflush(stdout);
              exit(2);
            }
          }
          fcntl(inputSource, F_SETFD, FD_CLOEXEC);
        }//end input redirect

        /* Handle output redireciont */
        if (strlen(outputReDir) != 0) { //check for output redirect
          //printf("Output redirected to: %s\n", outputReDir);
          outputTarget = open(outputReDir, O_WRONLY | O_CREAT | O_TRUNC, 0644); //truncate existing file, open for write only, create if does not exist, read/write for owner only
          if (outputTarget == -1) { //if error opening print error and set exit to 1
            perror("target open()");
            fflush(stdout);
            exit(1);
          }
          else { //
            //printf("outputTarget == %d\n", outputTarget);
            int result = dup2(outputTarget, 1);
            if (result == -1) { //if error redirecing stdin to output target
              perror("target dup2()");
              fflush(stdout);
              exit(2);
            }
          }
          fcntl(outputTarget, F_SETFD, FD_CLOEXEC);
        }//end output redirect

        //Execute command
        execvp(argArray[0], argArray);

        //if there is an error this executes otherwise this is not reached
        perror("execv");
        fflush(stdout);
        exit(2);
        break;
      
      default: //Parent process
        childPid = spawnpid;
      
         /*Exit process handling
          * Citation: information from Benjamin Brewster Youtube: www.youtube.com/watch?v=1R9h-H2UnLs */
        
        if (background == 0) { //Run process in foreground and capture exit status
          //printf("I am the parent. My pid = %d\n", getpid());
          spawnpid = waitpid(spawnpid, &childStatus, 0);
          if (WIFEXITED(childStatus)) { //if child process exited normally
            exitStatus = WEXITSTATUS(childStatus);
            *exitMethod = 0;
          }
          else { //If exited due to signal
            exitStatus = WTERMSIG(childStatus);
            *exitMethod = 1;
            print_status(2, *exitMethod);
          }
        //printf("Parent is done waiting for child with pid %d which exited and spawnpid %d\n", childPid, spawnpid);
        }
        else { //Run process in background 
          printf("background pid is %d\n", childPid);
          fflush(stdout);
          spawnpid = waitpid(childPid, &childStatus, WNOHANG);
        }
  }//end switch
  
  //printf("The process with pid %d is returning from main\n", getpid());
  return exitStatus;

} //end execute_command function

/**************************************************************
 * print_status
 * Description: Functions as status function, prints current exit status out
 * INPUT: exitVal - integer of exit value to print out
 *        exitMethod - integer value of the type of exit method, 0 for exit, 1 for signal terminated
 * OUTPUT: None
 * ************************************************************/

void print_status(int exitVal, int exitMethod) {
      //printf("Checking exit status \n");
      if (exitMethod == 0) {
        printf("exit value %i\n", exitVal);
      }
      else {
        printf("terminated by signal: %i\n", exitVal);
      }
      fflush(stdout);
} //end print_status function


/**************************************************************
 * handle_SIGTSTP
 * Description: handles SIGTSTP signal interrupt by toggling global variable
 * global_signal_toggle and prints message 
 * INPUT: signo - required argument but not used, ignore warnings from unused variable here
 * OUTPUT: None
 * ************************************************************/

void handle_SIGTSTP(int signo) {

  if (global_signal_toggle == 0) {  //If currently not in foreground only mode
    global_signal_toggle = 1;
    char* message = "\nEntering foreground-only mode (& is now ignored)\n:";
    write(STDOUT_FILENO, message, strlen(message));
  }
  else {
    global_signal_toggle = 0;
    char* message = "\nExiting foreground-only mode\n:";
    write(STDOUT_FILENO, message, strlen(message));
  }
  fflush(stdout); //Clear stdout after write!
  return;
}//end handle_SIGINT function

