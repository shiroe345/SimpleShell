#ifndef _POSIX_C_SOURCE
	#define  _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define N 1000 //command_maximun length
#define LSH_TOK_BUFSIZE 64 //initial string number
#define LSH_TOK_DELIM " \t\r\n" //delimiter

typedef struct command //use double linked list to implement stack(to record history)(this is node)
{
	char content[N];
	struct command *next, *pre;
}command;

typedef struct record// this is stack
{
	int size;
	command *head, *tail;
}record;

typedef struct cmd//used in pipe
{
  const char **argv;
}cmd;

record command_stack; //record command history
char *input; //used to record command in history
int string_num; //string number in one line;
int buff_size;  //command length

void command_new(record* command_stack, command* new_node){//add new command to history
	if(command_stack->size == 0){//empty
		command_stack->head = new_node;
		command_stack->tail = new_node;
		command_stack->size++;
	}
	else if(command_stack->size == 16){//full
		new_node -> next = command_stack->head;
		command_stack->head->pre = new_node;
		command_stack->head = new_node;

		command *new_tail = command_stack->tail->pre;//delete tail
		new_tail->next = NULL;
		free(command_stack->tail);
		command_stack->tail = new_tail;
	}
	else{
		new_node -> next = command_stack->head;
		command_stack->head->pre = new_node;
		command_stack->head = new_node;
		command_stack->size++;
	}
}

int lsh_execute(char **args);

char *lsh_read_line(void) //read a line
{
    char *line = NULL;
    size_t bufsize = 0; // have getline allocate a buffer for us

    while (getline(&line, &bufsize, stdin) == -1){ //get \t or nothing , getline will get the '\n' char
        printf(">>> & ");
    }
    buff_size = bufsize;
    memcpy(input , line, bufsize);
    return line;
}

char **lsh_split_line(char *line) //seperate the line to tokens
{
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    string_num = position;
    tokens[position] = NULL; //last be null for execvp
    return tokens;
}

int lsh_launch(char **args) //execute external commands ,doesn't need to use it now
{
    int pid, status;
    pid_t wpid;

    pid = fork();
    if (pid == 0) {  // Child process
        if (execvp(args[0], args) == -1) { //if exec success , the process will disappear
        perror("lsh"); //print error
        } 
        exit(EXIT_FAILURE); //exit 1
    } else if (pid < 0) { // Error forking
        perror("lsh");
    } else { // Parent process
        do {
        wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status)); //first is child successfully finished , second is no signal is catched in child
    }

    return 1;
}

/*
  Function Declarations for builtin shell commands:
 */
int lsh_help(char **args);
int lsh_cd(char **args);
int lsh_echo(char **args);
int lsh_record(char **args);
int lsh_replay(char **args);
int lsh_mypid(char **args);
int lsh_exit(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
    "help",
    "cd",
    "echo",
    "record",
    "replay",
    "mypid",
    "exit"
};

int (*builtin_func[]) (char **) = { //what is this
    &lsh_help,
    &lsh_cd,
    &lsh_echo,
    &lsh_record,
    &lsh_replay,
    &lsh_mypid,
    &lsh_exit
};

int lsh_num_builtins() { //builtins' number
  return sizeof(builtin_str) / sizeof(char *);
}

/*    Builtin function implementations.  */

int lsh_help(char **args)
{
    /*printf("-------------------------------------\n"
             "my_shell's commands:                 \n"
             "                                     \n"
             "1. help  : show all built-in command \n"
             "2. cd    : change directory          \n"
             "3. echo  : echo this string to stdout\n"
             "4. record: show command in history   \n"
             "5. replay: execute command in history\n"
             "6. mypid : show my pid               \n"
             "7. exit  : exit this shell           \n"
             "                                     \n"
             "-------------------------------------\n");*/
    char output_string[N] = {"-------------------------------------\nmy_shell's commands:                 \n\n 1. help  : show all built-in command \n 2. cd    : change directory          \n 3. echo  : echo this string to stdout\n 4. record: show command in history   \n 5. replay: execute command in history\n 6. mypid : show my pid               \n 7. exit  : exit this shell           \n \n -------------------------------------\n"};
    
    printf("%s\n" , output_string);
    return 1;
}

int lsh_cd(char **args) //cd here
{
    if (args[1] == NULL) { //no arguments
        return 1;
    } else {
        if (chdir(args[1]) != 0) { //no such directory
            perror("lsh");
        }
    }
    return 1;
}

int lsh_echo(char **args){ //echo the command
    if(args[1] && strcmp(args[1], "-n" )== 0){
        int len = 2;
        while(args[len]) printf("%s ",args[len++]);
    }
    else{
        int len = 1;
        while(args[len]) printf("%s ",args[len++]);
        printf("\n");
    }
    return 1;
}

int lsh_record(char **args){
    char output_string[17*N] = {"history commands: \n"}; //used to output
    char str[N];//used to save every single line

    command *tmp = command_stack.tail;
    for(int i = 1;i<=command_stack.size;i++){
        snprintf(str, sizeof(str), "%2d: %s",i,tmp->content); //use the format to save string for output
        strcat(output_string , str);
        tmp = tmp -> pre;
    }

	printf("%s", output_string);
}

int lsh_replay(char **args){  //no use now
    return 0; //shouldn't enter here
}

int lsh_mypid(char **args){
    if(!args[1]) return 1;//no args

    int pid = getpid();
    FILE* f;
    char path[100] = {"/proc/"};

    if(strcmp(args[1], "-i") == 0){
        printf("%d\n",pid);
    }
    else if(args[2] && strcmp(args[1], "-p") == 0){
        strcat(path, args[2]);
        strcat(path, "/status");
        f = fopen(path,"r");
        int i = 0;
        char line[10000];
    
        while(i<7){//get the seven line in directory
            fgets(line, sizeof(line), f);
            i++;
        }
        char *token = strtok(line, " \t"); //get the second
        token = strtok(NULL, " \t");
        printf("%s",token);
        fclose(f);
    }
    else if(args[2] && strcmp(args[1], "-c") == 0){
        /*set path*/
        strcat(path, args[2]);
        strcat(path, "/task/");
        strcat(path, args[2]);
        strcat(path, "/children");

        f = fopen(path,"r");
        int i = 0;
        char c_pid[1000];//child pid
    
        while(fscanf(f,"%s",c_pid) != EOF){
            printf("%s ",c_pid);
        }
        printf("\n");
        fclose(f);
    }
    else{
        printf("wrong args!\n");
    }
    return 1;
}

int lsh_exit(char **args){
    printf("exit shell\n");
    exit(0);
}
/*        builins end      */

int lsh_save_history(char* line){ //check if replay and save to record
    if(!line || strlen(line) < 1 || line[0] == '\n') return 0;//nothing is input

    char *tmp  = (char*)malloc(buff_size + 1) ,*tmp2 = (char*)malloc(buff_size + 1);//malloc some strings to be length of command line
    strcpy(tmp, line);
    strcpy(tmp2, line);

    char *token = strtok(tmp ,"|");//use pipe as delim to find first command line
    if(!token) return 0; //no command

    char *token2 , *args;
    token2 = strtok(tmp2," \t\n"); //check if first string of command is replay
    if(token2 && strcmp(token2, "replay") == 0) args = strtok(NULL," \t\n");
    else return 0; //nothing before '|'
    
    int num = 0; //change number after replay from string to int
    if(args){
        if(strlen(args) == 1 && args[0] >= '1' && args[0] <= '9'){
            num = args[0] - '0';
        }
        else if(strlen(args) == 2 && args[0] >= '1' && args[0] <= '9' && args[1] >= '0' && args[1] <= '9'){
            num = (args[1] - '0') + (args[0] - '0') * 10; 
        }
        else return 1; //replay illegal
    }
    else return 1; //replay illegal
    //printf("%d",num);
    if(num > command_stack.size) return 1; //larger than stack size

    char *ret = (char*)malloc(N);
    command *t = command_stack.tail;

    for(int i = 1;i<num;i++)//choose the 'num' command
		t = t->pre;

    char* tmp3 = (char*)malloc(strlen(line)+ 1);

    strcpy(tmp3, line);
    strcpy(line, t->content);

    line[strlen(line)-1] = '\0'; //no need '\n
    char *token3 = strtok(tmp3," \t\n"); //pre 2 is no need
    token3 = strtok(NULL, " \t\n");

    token3 = strtok(NULL, " \t\n");
   
    while(token3){
        strcat(line, " ");
        strcat(line, token3);
        token3 = strtok(NULL, " \t\n");
    }
    if(line[strlen(line)-1] != '\n'){
        strcat(line, "\n\0");
    }

    return 0;
    /* free */
    //free(ret);
    //free(tmp);
    //free(tmp2);
    //free(token2);
    //free(args);
}

int lsh_parse_pipe(char* line, char** lines){ //parse the string using "|",return number of commands
    int number = 0;//number of commands after pipeling
    char tmp[N];tmp[0] = '\0';
    int len = 0;//len of tmp
    for(int i = 0;i<strlen(line);i++){
        if(line[i] != '|')
            tmp[len++] = line[i];
        else{
            lines[number] = (char*)malloc(sizeof(char) * len + 1);
            tmp[len] = '\0';
            strcpy(lines[number], tmp);
            len = 0;
            number++;
            tmp[0] = '\0';
        }
    }
    if(len > 0){ //tmp len > 0
        lines[number] = (char*)malloc(sizeof(char) * len + 1);
        tmp[len] = '\0';
        strcpy(lines[number], tmp);
        len = 0;
        number++;
        tmp[0] = '\0';
    }
    return number;
}

int lsh_execute(char **args)//seperate external commands and builtins ,but doesn't use it now
{
    int i;

    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0)   //builtin function
            return (*builtin_func[i])(args);
    }

    return lsh_launch(args); //external function
}

int lsh_pipe_excute(char** cmds[],int size){ //excute pipe program
    int fd[(size-1) * 2]; //need size-1 pipes(file descriptor)
    for(int i = 0;i<size-1;i++)
        pipe(fd + i*2); //need size-1 pipe()
    

    /* first done by current process(if builtins) */
    for(int i = 0;i<size;i++){

        /*********** redirection ***********/
        int in_num = dup(STDIN_FILENO); //in_num = the number pointed by stdin
        int out_num = dup(STDOUT_FILENO); //out_num = the number pointed by stdout

        int in = 0,out = 0;
        char input_file[1000],output_file[1000];
        int pos1,pos2;
        if(i == 0){
            for(int j = 0;cmds[i][j+1];j++){
                if(strcmp(cmds[i][j], "<") == 0){ //find output redirection
                    in = 1;
                    strcpy(input_file, cmds[i][j+1]);
                    pos1 = j;
                    break;
                }
            }
        }
        if(i == size - 1){ //check output redirection
            for(int j = 0;cmds[i][j+1];j++){
                if(strcmp(cmds[i][j], ">") == 0){ //find output redirection
                    out = 1;
                    strcpy(output_file, cmds[i][j+1]);
                    pos2 = j;
                    break;
                }
            }
        }

        if(in){
            // int fd = open(input_file, O_RDONLY ,0);
            // if(fd < 0){
            //     printf("no this file!\n");
            //     return 1;
            // }
            // dup2(fd, STDIN_FILENO);//fd[stdin] point to where pointed by fd[p]
            // close(fd);
            cmds[i][pos1] = NULL;
            cmds[i][pos1+1] = NULL;
        }
        if(out){
            // int fd1 = open(output_file, O_WRONLY , 1); 
            // if(fd1 < 0) fd1 = creat(output_file, 0644); //if not exist, creat
            // dup2(fd1, STDOUT_FILENO);
            // close(fd1);
            cmds[i][pos2] = NULL; //file name and ">" be NULL
            cmds[i][pos2+1] = NULL;
        }

        /*********** redirection end ***********/

        if(i == 0){ //first be done by parent if builtin
            //int stdout_num = dup(STDOUT_FILENO); //stdout_num = the number pointed by stdout
            int b = 0;
            for (int j = 0; j < lsh_num_builtins(); j++) {
                if (strcmp(cmds[i][0], builtin_str[j]) == 0){  //builtin function
                    if(out){//only one command exist , and need output redirection
                        int fd1 = creat(output_file, 0644); 
                        dup2(fd1, STDOUT_FILENO);
                        close(fd1);
                    }
                    else dup2(fd[1],STDOUT_FILENO);  //stdout point to where fd[1] point
                    (*builtin_func[j])(cmds[i]);
                    b = 1;
                    break;
                }
            }
            dup2(out_num, 1); //point back to stdout (dup2(A,B) -> *B = *A, B point to where pointed by A)
            if(b) continue; //if is builtin
        }

        pid_t pid;

        pid = fork();
        if(pid < 0){ //fork error
            printf("error!\n");
            return 1;
        }
        else if (pid == 0) { // Child
            // 讀取端
            if (i != 0) {
                // 用 dup2 將 pipe 讀取端取代成 stdin
                dup2(fd[(i - 1) * 2], STDIN_FILENO);
            }
    
            // 用 dup2 將 pipe 寫入端取代成 stdout
            if (i != size-1) {
                dup2(fd[i * 2 + 1], STDOUT_FILENO);
            }

            // 關掉之前一次打開的
            for (int j = 0; j < (size -1)*2; j++) {
                close(fd[j]);
            }

            if(in){
                int fd = open(input_file, O_RDONLY ,0);
                if(fd < 0){
                    printf("no this file!\n");
                    return 1;
                }
                dup2(fd, STDIN_FILENO);//fd[stdin] point to where pointed by fd[p]
                close(fd);
            }

            if(out){ //if output redirection 
                int fd1 = creat(output_file, 0644); 
                dup2(fd1, STDOUT_FILENO);
                close(fd1);
            }

            /* builtin */
            for (int j = 0; j < lsh_num_builtins(); j++) {
                if (strcmp(cmds[i][0], builtin_str[j]) == 0){  //builtin function
                    (*builtin_func[j])(cmds[i]);
                    exit(0);
                }
            }
            /* externel */
            execvp(cmds[i][0],cmds[i]);
            fprintf(stderr, "Cannot run %s\n", *cmds[i]);

        } else { // Parent
            if(i>0) close(fd[(i - 1) * 2]);     // 前一個的寫
            if(i>0) close(fd[(i - 1) * 2 + 1]); // 當前的讀
        }
        waitpid(pid, NULL, 0); //need to add this line or will bug ,need to wait the last child process finish

    }   
    return 1;
}

void lsh_loop(void) //keep shell working
{
    char *line ,**lines = (char**)malloc(sizeof(char*) * N);  //input line
    char **args; //arguments
    char **cmds[N]; //pipe the commands
    int status;  //current status
    do {
        printf(">>> & ");
        line = lsh_read_line(); //read a line

        if(lsh_save_history(line)){ //if replay illegal
            printf("replay: wrong args\n");
            status = 1;
            free(line);
            continue;
        }//save to history
        
        /* save command */
        command *new_node = (command*)(malloc(sizeof(command))); //add new instruction
		strcpy(new_node->content, line);
		command_new(&command_stack, new_node);

        int size = lsh_parse_pipe(line, lines); //parse one line into lines by |

        for(int i = 0;i<size;i++){
            args = lsh_split_line(lines[i]); //ex {"ls","-al"}
            //if(args[1] == NULL) printf(1); //why this line will be bug >>> can't print 1
            cmds[i] = args;
        }

        /* check background if need */
        int i;
        for(i = 0;cmds[size-1][i+1];i++);
        if(strcmp(cmds[size-1][i] ,"&") == 0){ //have background
            pid_t pid;

            cmds[size-1][i] = NULL; //"&" be NULL
            pid = fork();

            if(pid < 0){
                printf("fork error!\n");
                return;
            }
            else if(pid == 0){
                status = lsh_pipe_excute(cmds, size);
                exit(0);
            }
            else{
                status = 1;
                printf("[Pid]: %d\n",pid);
            }
            waitpid(pid, NULL, 0);
        }
        else status = lsh_pipe_excute(cmds, size); //execute the commands
        
        for(int i = 0;i<size;i++) free(cmds[i]);
        free(line);

        
    } while (status); 
}

void Init(){ //initialization
    command_stack.size = 0;
    input = (char*)(malloc(sizeof(char) * N));

    printf("=========================================\n"//print welcome message
			"* Welcome to my shell:                 *\n"
			"* Type help to see built-in command    *\n"								
			"* If you want to do things below:      *\n"
			"* # redirection: '>' or '<'            *\n"
			"* # pipe: '|'                          *\n"
			"* # background: '&'                    *\n"
			"* Please seperate them by space        *\n"
			"*                                      *\n"
			"* Have fun !                           *\n"
			"========================================\n");
}

int main(int argc, char **argv){
    struct sigaction sigchld_action = {.sa_handler = SIG_DFL,.sa_flags = SA_NOCLDWAIT};// 處理 SIGCHLD，可以避免 Child 疆屍程序

    Init();

    lsh_loop();

    return EXIT_SUCCESS;
}
	
