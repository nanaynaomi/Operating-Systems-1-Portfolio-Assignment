/*	
Naomi Grant
Assignment 3 - Smallsh
Resources used: Code from Explorations, read Piazza questions, MS Teams, TA office hours
*/


#include <stdlib.h>
#include <sys/types.h> // pid_t
#include <sys/wait.h>
#include <unistd.h> // fork
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>



bool fgOnlyMode;
bool printFGmsg;

struct userIn
{
	char* command;
	char* args[512]; 
	char* inputFile;
	char* outputFile;
	int background; // Store it as an int of either 0 or 1
};


/* #########################################
			PROCESS / PARSE INPUT
#############################################*/

/*
* Processes the input the user entered to the command line into command, args, inputFile, and outputFile,
* and sets background to 1 or 0 depending on if & is at the end.
*/
struct userIn* processInput(char* currLine, int fgOnlyMode)
{
	struct userIn* cmdLine = malloc(sizeof(struct userIn));
	char* saveptr;				// For use with strtok_r

	cmdLine->background = 0;	// Initially let this be 0 (to indicate false)

	char* token = strtok_r(currLine, " ", &saveptr);			// The first token is the command
	cmdLine->command = calloc(strlen(token) + 1, sizeof(char));	// Use calloc to allocate memory
	strcpy(cmdLine->command, token);							// Copy token to cmdLine->command

	token = strtok_r(NULL, " ", &saveptr);		// Get next token

	int i = 0;					// Used with placing tokens in args[]
	while (token != NULL)		// While we haven't reached the end of the line
	{
		if (strcmp(token, "<") == 0)	// If token is "<" (for input)
		{
			token = strtok_r(NULL, " ", &saveptr);		// Get next token (the input file)
			if (token != NULL) {
				cmdLine->inputFile = calloc(strlen(token) + 1, sizeof(char));
				strcpy(cmdLine->inputFile, token);
			}
			token = strtok_r(NULL, " ", &saveptr);	// Get next token
		}
		else if (strcmp(token, ">") == 0) // If token is ">" (for output)
		{
			token = strtok_r(NULL, " ", &saveptr);		// Get next token (the output file)
			if (token != NULL) {
				cmdLine->outputFile = calloc(strlen(token) + 1, sizeof(char));
				strcpy(cmdLine->outputFile, token);			// Copy output file name to cmdLine->outputFile
			}
			token = strtok_r(NULL, " ", &saveptr);	// Get next token
		}
		else if (strcmp(token, "&") == 0)
		{
			if (fgOnlyMode == false)
			{
				char* temp = token;
				token = strtok_r(NULL, " ", &saveptr);	// Get next token

				if (token == NULL)	// If there is NO more text after "&" (it is last char on line)
				{
					cmdLine->background = 1;	// Set background to 1 (to indicate true)
				}
				else {	// Otherwise, take the & token to be a string.
					cmdLine->args[i] = calloc(strlen(temp) + 1, sizeof(char));
					strcpy(cmdLine->args[i], temp);		// Copy argument to cmdLine->args[i]
					i++;
					cmdLine->args[i] = calloc(strlen(token) + 1, sizeof(char));
					strcpy(cmdLine->args[i], token);		// Copy argument to cmdLine->args[i]
					i++;									// Increment i
				}
			}
			else
			{
				token = strtok_r(NULL, " ", &saveptr);	// Get next token
			}
			
		}
		else {
			cmdLine->args[i] = calloc(strlen(token) + 1, sizeof(char));
			strcpy(cmdLine->args[i], token);		// Copy argument to cmdLine->args[i]
			i++;									// Increment i
			token = strtok_r(NULL, " ", &saveptr);	// Get next token
		}
	}
	return cmdLine;
}


/* #########################################
			HANDLE $$
#############################################*/

/* 
* Searches the input string for occurances of $$ and replaces them with the PID.
*/
void replaceDollarSigns(char inputString[])
{
	char dollarSigns[] = "$$";	// The string we're looking for
	pid_t pid = getpid();		// The pid to replace it with
	char thepid[30];
	sprintf(thepid, "%d", pid);	// Make pid a string
	char buffer[256];
	char* pInput = inputString;	// Initialize pointer
													// strstr returns a pointer to first occurance of $$ in string
	while ((pInput = strstr(pInput, dollarSigns)))	// Use strstr to find all occurances of $$
	{
		strncpy(buffer, inputString, pInput - inputString);	
		buffer[pInput - inputString] = '\0';
		strcat(buffer, thepid);
		strcat(buffer, pInput + strlen(dollarSigns));
		strcpy(inputString, buffer);
		pInput++;
	}
}

/*
* Calls replaceDollarSigns on command, args, inputFile and outputFile.
*/
void handleDollarSigns(struct userIn* cmdLine)
{
	// Command
	replaceDollarSigns(cmdLine->command);		// Replace any $$ in command with pid

	// args
	int j = 0;
	while (cmdLine->args[j] != NULL)
	{
		replaceDollarSigns(cmdLine->args[j]);	// Replace any $$ in args with pid
		j++;
	}

	// input file
	if (cmdLine->inputFile != NULL) {
		replaceDollarSigns(cmdLine->inputFile);	// Replace any $$ in inputFile with pid
	}

	// output file
	if (cmdLine->outputFile != NULL) {
		replaceDollarSigns(cmdLine->outputFile); // Replace any $$ in outputFile with pid
	}

}





/*#########################################
			Redirect I/O
#############################################*/

/*
* Redirects the input/output to inputFile/outputFile if needed. Background processes without
* redirection specified are sent to dev/null.
*/
void redirectIO(struct userIn* cmdLine)
{
	int sourceFD;
	int targetFD;
	int result;


	// ### Input ###

	if (cmdLine->inputFile != NULL) {
		sourceFD = open(cmdLine->inputFile, O_RDONLY);	// Open source file	
		if (sourceFD == -1) {
			perror("source open()");
			exit(1);
		}
		// Redirect stdin to source file
		result = dup2(sourceFD, 0);
		if (result == -1) {
			perror("source dup2()");
			exit(1);
		}
	}
	else if (cmdLine->background == 1)
	{
		sourceFD = open("/dev/null", O_RDONLY);			// Open source file
		if (sourceFD == -1) {
			perror("source open()");
			exit(1);
		}
		// Redirect stdin to source file
		int result = dup2(sourceFD, 0);
		if (result == -1) {
			perror("source dup2()");
			exit(1);
		}
	}

	// ### Output ###

	if (cmdLine->outputFile != NULL) {
		targetFD = open(cmdLine->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (targetFD == -1) {
			perror("open()");
			exit(1);
		}
		// Use dup2 to point FD 1, i.e., standard output to targetFD
		result = dup2(targetFD, 1);
		if (result == -1) {
			perror("dup2");
			exit(1);
		}
	}
	else if (cmdLine->background == 1)
	{
		targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (targetFD == -1) {
			perror("open()");
			exit(1);
		}
		// Use dup2 to point FD 1, i.e., standard output to targetFD
		result = dup2(targetFD, 1);
		if (result == -1) {
			perror("dup2");
			exit(1);
		}
	}

	
}


/* #########################################
			COMMAND IS -> OTHER
#############################################*/

/*
* If the command is not exit, cd, or status, this function runs it using execvp.
*/
void otherCommand(struct userIn* cmdLine, int *backPids, int *exitStatusF)
{
	int childStatus;
	pid_t spawnpid = -5;
	int arrIndex = 2;
	char* arrForExec[257] = { cmdLine->command, cmdLine->args[0] };
	int argNum = 0;

	// ###### Background command ######

	if (cmdLine->background == 1)	 
	{
		spawnpid = fork();
		switch (spawnpid)
		{
		case -1:	// If fork fails it will return -1
			perror("fork() failed!");
			exit(1); 
			break;
		case 0: { // Child
			struct sigaction SIGINT_action = { 0 };
			struct sigaction SIGTSTP_action = { 0 };

			// Fill out the SIGINT_action and SIGTSTP_action structs with SIG_IGN
			SIGINT_action.sa_handler = SIG_IGN;	
			sigaction(SIGINT, &SIGINT_action, NULL);

			SIGTSTP_action.sa_handler = SIG_IGN;
			sigaction(SIGTSTP, &SIGTSTP_action, NULL);


			redirectIO(cmdLine);	// Redirect I/O

			while (cmdLine->args[argNum] != NULL)	// Creates an array of command, arg1, arg2, ... , argn, NULL
			{
				argNum++;
				arrForExec[arrIndex] = cmdLine->args[argNum];
				arrIndex++;
			}
			execvp(arrForExec[0], arrForExec);	// Run command using execvp and array from above
			perror("execvp");					// If execvp fails, print this to the user.
			exit(1);							// Exits with code 1 if command not found
			break;
		}
		default: // Parent
			printf("background pid is %d\n", (int)spawnpid); // Print background PID to user
			fflush(stdout);
			
			// Store non-completed background process id to array
			for (int i = 0; i < 200; i++)	// Loops through array of background pids
			{
				if (backPids[i] == 0)		// Finds first "empty slot" in the array (i.e. where it is 0)
				{
					backPids[i] = spawnpid;	// Add pid to array of background pids currently running
					break;
				}
			}
			break;
		}
	}
	else	// ####### Foreground command #######
	{
		spawnpid = fork();
		switch (spawnpid)
		{
		case -1:	// If fork fails it will return -1
			perror("fork() failed!");
			fflush(stdout);
			exit(1); 
			break;

		case 0: {	// ## Child ##

			struct sigaction SIGINT_action = { 0 };
			struct sigaction SIGTSTP_action = { 0 };

			// Fill out the SIGINT_action struct with SIG_DFL
			SIGINT_action.sa_handler = SIG_DFL;			
			sigaction(SIGINT, &SIGINT_action, NULL);

			// Fill out the SIGTSTP_action struct with SIG_IGN
			SIGTSTP_action.sa_handler = SIG_IGN;
			sigaction(SIGTSTP, &SIGTSTP_action, NULL);


			redirectIO(cmdLine);	// Redirect I/O

			while (cmdLine->args[argNum] != NULL)	// Creates an array of command, arg1, arg2, ... , argn, NULL
			{
				argNum++;
				arrForExec[arrIndex] = cmdLine->args[argNum];
				arrIndex++;
			}
			execvp(arrForExec[0], arrForExec);		// Run command using execvp and array from above
			perror("execvp");						// If execvp fails, print this to the user.
			exit(1);								// Exits with code 1 if command not found
			break;
		}
		default: // ## Parent ##
			spawnpid = waitpid(spawnpid, &childStatus, 0);

			// Set exitStatusF with exit status
			if (WIFSIGNALED(childStatus)) {
				*exitStatusF = WTERMSIG(childStatus);
			}
			else {
				*exitStatusF = WEXITSTATUS(childStatus);
			}
			break;
		}
	}
}


/* #########################################
	COMMAND IS -> EXIT, STATUS, CD
#############################################*/

/*
* Looks at command . If the command entered is exit, status, or cd, it is handled here.
* Otherwise, it calls the otherCommand function.
*/
void theCommand(struct userIn* cmdLine, int *backPids, int *exitStatusF)
{
	if (!strcmp(cmdLine->command, "exit"))  // #### EXIT ####
	{
		// Kill any currently running background processes and jobs
		for (int i = 0; i < 200; i++)
		{
			int childStatus;
			if (i != 0)
			{
				kill(backPids[i], SIGTERM);
				waitpid(backPids[i], &childStatus, 0);
			}
		}
		exit(0);	// Exit with code 0
	}
	else if (!strcmp(cmdLine->command, "status"))  // #### STATUS ####
	{
		// Prints out either the exit status or the terminating signal of the 
		// last foreground process run by the shell.

		// If no foreground commands have been run yet:
		if (*exitStatusF == 9) 
		{
			printf("exit value 0\n");
			fflush(stdout);
		}
		else // else if a foreground command has run:
		{
			printf("exit value %d\n", *exitStatusF);
			fflush(stdout);
		}	
	}
	else if (!strcmp(cmdLine->command, "cd"))		// #### CD ####
	{
		if (cmdLine->args[0] != NULL) // If there is an argument
		{
			int dirChange = chdir(cmdLine->args[0]); // Change to directory specified in input line
			if (dirChange == -1) {
				perror("Non-existent directory\n");
				exit(1);
			}
		}
		else // If it's just 'cd'			
		{
			char* homeEnv = getenv("HOME");	// Change to the directory specified in the HOME environment variable.
			chdir(homeEnv);
		}
	}
	else		// #### OTHER ####
	{
		// A command other than exit, cd, or status.
		otherCommand(cmdLine, backPids, exitStatusF);
	}
}

/*#########################################
				SIGNALS
#############################################*/
void handle_SIGTSTP(int signo) 
{
	printFGmsg = true;
	// Set bool fgOnlyMode (forground only mode)
	if (fgOnlyMode) 
	{
		fgOnlyMode = false;
	}
	else 
	{
		fgOnlyMode = true;
	}
}


void handle_SIGINT(int signo) 
{
	char* message = " terminated by signal 2\n";
	write(STDOUT_FILENO, message, 30);

}




/* #########################################
			MAIN
#############################################*/

int main() 
{
	setbuf(stdout, NULL);
	fgOnlyMode = false;
	printFGmsg = false;

	struct sigaction SIGINT_action = { 0 }, SIGTSTP_action = { 0 };

	// Fill out the SIGINT_action struct
	SIGINT_action.sa_handler = SIG_IGN;			// Register handle_SIGINT as the signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);	// Install our signal handler

	// Fill out the SIGTSTP_action struct
	SIGTSTP_action.sa_handler = handle_SIGTSTP;	// Register handle_SIGTSTP as the signal handler
	sigfillset(&SIGTSTP_action.sa_mask);		// Block all catchable signals while handle_SIGTSTP is running
	SIGTSTP_action.sa_flags = SA_RESTART;		// No flags set
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);	// Install our signal handler
	
	char inputLine[2048];		// Represents the full line entered
	int meow = 1;		
	int currBackPids[200];		// Array of pids of current background processes running
	memset(currBackPids, 0, sizeof(currBackPids));	// Initialize everything in currBackPids to 0
	int exitStatusF = 9; 

	while (meow)
	{
		// Print message for Ctrl^Z (SIGTSTP) if needed
		if (printFGmsg == true) {
			if (fgOnlyMode) {
				printf("Entering foreground-only mode (& is now ignored)\n");
				fflush(stdout);
			}
			else {
				printf("Exiting foreground-only mode\n");
				fflush(stdout);
			}
			printFGmsg = false;
		}
		
		// Check if any background processes are complete and if so, print this info before prompting user.
		for (int i = 0; i < 200; i++)
		{
			int childStatus;
			if (currBackPids[i] == 0)
			{
				continue;
			}
			else if (waitpid(currBackPids[i], &childStatus, WNOHANG))
			{
				if (WIFSIGNALED(childStatus)) {
					printf("background pid %d is done: terminated by signal %d\n", currBackPids[i], WTERMSIG(childStatus));
					fflush(stdout);
				}
				else {
					printf("background pid %d is done: exit value %d\n", currBackPids[i], WEXITSTATUS(childStatus));
					fflush(stdout);
				}
				currBackPids[i] = 0; // remove it from array
			}
		}

		printf(": ");
		fflush(stdout);
		
		fgets(inputLine, 2048, stdin);
		inputLine[strcspn(inputLine, "\n")] = '\0';

		// Make sure line is not a comment or blank line before running - strcmp returns 0 if the two strings being compared are equal
		if ( (inputLine[0] != '#') && (inputLine[0] != '\0') ) // If first char is not # and not '\n'
		{
			struct userIn* cmdLine = processInput(inputLine, fgOnlyMode);

			handleDollarSigns(cmdLine);

			theCommand(cmdLine, currBackPids, &exitStatusF);
		}

	} 
	return 0;
}