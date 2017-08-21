#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h> 
#include <signal.h>

int numberOfTokens;

int fd;
FILE* BatchFile;
//Forward declarations
void mysh_loop();
void executeSpecial(char** spec ,char* redirpath, int amp);

//Error handling
void error(){
	char error_message[30] = "An error has occurred\n"; 
	write(STDERR_FILENO, error_message, strlen(error_message));
	mysh_loop();
}
void errorBatch(){
	char error_message[30] = "An error has occurred\n"; 
	write(STDERR_FILENO, error_message, strlen(error_message));
	exit(1);
}
void errorExecvp(){
	char error_message[30] = "An error has occurred\n"; 
	write(STDERR_FILENO, error_message, strlen(error_message));
	exit(1);
	mysh_loop();
}


void file_redirection(char* path){ 
	char* cwd = malloc(4096);
	int fd;
	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);

	//Unable to create redirection file.
	if (fd < 0) {
		error();
	}
	getcwd(cwd, 4096);
	write(fd, cwd, strlen(cwd));
	close(fd);
	mysh_loop();
}

char* readLine()
{
	char * line = malloc(10000);
	size_t  n;
	ssize_t chars;
	//Print each line in batch file to command line
	if (BatchFile)
	{
		chars = getline(&line, &n, BatchFile);
		write(STDOUT_FILENO,line,strlen(line));
		//Empty batch file
		if (chars == -1)
		{
			exit(1);
		}
	}

	else
	{
		//Build input from command line 
		chars = getline(&line, &n, stdin);
	}


	if (chars >= 1)
	{
		//Verify char limit
		if(chars <= 513)
		{
			if ((line)[chars - 1] == '\n') 
			{
				(line)[chars - 1] = '\0';
				return line;
			}
		}
		else
		{
			//if input is too long
			free(line);
			error();
		}
	}
	else
	{
		return NULL;
	}
}

char** parse(char* LineOfText){
	numberOfTokens=0;
	char tokenDelims[] = " \t\n"; 
	char* copy = malloc(513);
	char* token;
	char** tokens = malloc(513);
	int position=0;

	//Prevent segfault
	if (LineOfText != NULL)
	{
		copy = strdup(LineOfText);
		//more than one >
		if(strchr(LineOfText, '>') != strrchr(LineOfText, '>'))
		{
			error();
		}
		//more than one &
		if(strchr(LineOfText, '&') != strrchr(LineOfText, '&'))
		{
			error();
		}
	}
	//Build char** tokens using delimiters
	token = strtok(copy, tokenDelims);
	while (token != NULL) {
		tokens[position] = token;
		position++;
		token = strtok(NULL, tokenDelims);
		numberOfTokens++;
	}
	return tokens;
}


int executeBuiltIn(char** builtInCommand){
	int cdi;
	char** wait = malloc(513);

	if (strcmp(builtInCommand[0], "exit") == 0) {
		exit(0);
	}

	if (strcmp(builtInCommand[0], "pwd") == 0) {
		char* cwd = malloc(4096);
		char* dir = malloc(4096);

		getcwd(cwd, 4096);
		if ((builtInCommand[1] != NULL && strcmp(builtInCommand[1], ">") == 0))
		{
			file_redirection(builtInCommand[2]);
		}
		else {
			printf("%s\n", cwd);
			return 1;
		}
		free(cwd);
		free(dir);
		return 1;
	}

	if (strcmp(builtInCommand[0], "cd") == 0 && builtInCommand[1] != NULL) {
		cdi = chdir(builtInCommand[1]);
		if (cdi == -1){
			//User gives invald directory
			error();
		}
		return 1;
	}
	if (strcmp(builtInCommand[0], "cd") == 0 ){ 
        //Move to home directory
		chdir(getenv("HOME"));
		return 1;
		
	}
	if (strcmp(builtInCommand[0], "wait") == 0 ){ 
		wait[0] = "wait";
		executeSpecial(wait, NULL, 0);
		return 1;
	}

	return 0;
}

void executeSpecial(char** specialCommand, char* redirectionPath, int ampersand){
	pid_t pid;
	pid_t wpid;
	int status;

	if (specialCommand[0] == "wait"){
		do 
		{
			wpid = waitpid(-1, &status, WUNTRACED);
		}
		while ( !WIFSIGNALED(status) &&  !WIFEXITED(status) );
	}

	pid = fork();
	//successful fork
	if (pid == 0) 
	{
		int fd = open(redirectionPath, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
		dup2(fd, 1);
		if (execvp(specialCommand[0], specialCommand) == -1) {
			if (specialCommand[0] != "wait"){
				errorExecvp();				
			}
			
		}

	}
	//error forking
	else if (pid < 0) 
	{
		error();
	}

	else 
	{
		//If & is not there, then wait
		if (!ampersand)
		{
			do 
			{
				wpid = waitpid(pid, &status, WUNTRACED);
			}
			while ( !WIFSIGNALED(status) &&  !WIFEXITED(status) );
		}
	}
}

void mysh_loop(){
	int ampersand=0;
	while(1) {
		int i=0;
		int builtin =0;

		char* line = malloc(513);
		char* redirectionPath = malloc(513);

		char** pythonArgs = malloc(513);
		char** tokenArgs;
		char** tokenArgsNoSymbols = malloc(513);

		if(!BatchFile && !ampersand){
			printf("mysh> ");
		}

		line = readLine();
		tokenArgs = parse(line);

        //If user presses enter without typing command
		if (tokenArgs[0] == NULL){
			mysh_loop();
		}

		//If user enters incorrect > and/or & syntax
		if ( strchr(line, '&') != NULL && (strcmp(tokenArgs[numberOfTokens-1],"&") != 0 || strcmp(tokenArgs[numberOfTokens-2],">") == 0) )
		{
			error();
		}

		if (strchr(line, '>') != NULL)
		{
			if (numberOfTokens > 2) 
			{

				if( (strcmp(tokenArgs[numberOfTokens-2],">") != 0 && strcmp(tokenArgs[numberOfTokens-1],"&") != 0 )|| 
					

					(strcmp(tokenArgs[numberOfTokens-3],">") != 0 && strcmp(tokenArgs[numberOfTokens-1],"&") == 0) ) 
				{
					error();
				}
			}
			else
			{
				error();
			}
			
		}
		//Python command handling, look for .py at the end of first arg
		if (strlen(tokenArgs[0]) >= 3 && *(tokenArgs[0] + (strlen(tokenArgs[0]) - 3)) == '.' && *(tokenArgs[0] + (strlen(tokenArgs[0]) - 2)) == 'p' && *(tokenArgs[0] + (strlen(tokenArgs[0]) - 1)) == 'y')
		{
			if (tokenArgs[1] != NULL){
				if (strcmp(tokenArgs[numberOfTokens-1],"&") == 0 || strcmp(tokenArgs[numberOfTokens-1],"&") == 0){
					ampersand=1;
				}
			}
			pythonArgs[0] = "python";
			pythonArgs[1] = tokenArgs[0];
			for (i=2; i<=numberOfTokens; i++) {
				if (strcmp(tokenArgs[i-1],"&") != 0) 
				{
					pythonArgs[i] = tokenArgs[i-1];
				}
			}
			executeSpecial(pythonArgs, NULL, ampersand);
			mysh_loop();
		}

		builtin = executeBuiltIn(tokenArgs);
		
        //Creates a new char** without symbols for execvp
		for (i=0; i < numberOfTokens; i++) {
			ampersand=0;
			if (strcmp(tokenArgs[i],">") == 0)
			{
				redirectionPath = tokenArgs[i+1];
				i++;
				continue;
			}
			if (strcmp(tokenArgs[i],"&") == 0)
			{
				ampersand = 1;
				break;
			}
			tokenArgsNoSymbols[i] = tokenArgs[i];
		}
		
		if (!builtin) 
		{
			executeSpecial(tokenArgsNoSymbols, redirectionPath, ampersand);
		}

	}

}
void handle_batch(char* batchFile){
	BatchFile = fopen(batchFile, "r");
	if (!BatchFile){
		errorBatch();
	}
	mysh_loop();
}
int main(int argc, char* argv[]){
	//If there is batch file provided
	if (argc == 2){
		handle_batch(argv[1]); 
	}
	//Main program loop
	else{
		mysh_loop();
	}
	return 1;
}