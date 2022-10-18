#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_SUB_COMMANDS  5
#define MAX_ARGS  10

struct SubCommand{
	char *line;
	char *argv[MAX_ARGS];
};

struct Command{
	struct SubCommand sub_commands[MAX_SUB_COMMANDS];
	int num_sub_commands;
	char *stdin_redirect;
	char *stdout_redirect;
	int background;	
};

void ReadArgs(char *in, char **argv,int size){
	char* token=strtok(in, " ");
	int i=0;
	while(token!=NULL&&i<size){
		argv[i]=strdup(token);
		i++;
		token=strtok(NULL," ");
	}
	argv[i]=NULL;
}

void ReadCommand(char *line, struct Command *command){
	char *t;
	char *copy;
	t=strtok(line,"|");
	int num=0;
	while(t!=NULL&&num<MAX_SUB_COMMANDS){
		copy=strdup(t);
		command->sub_commands[num].line=copy;
		num++;
		t=strtok(NULL,"|");
	}
	command->num_sub_commands=num;
	int i;
	for(i=0;i<num;i++){
		ReadArgs(command->sub_commands[i].line,command->sub_commands[i].argv,MAX_ARGS);
	}
}

void ReadRedirectsAndBackground(struct Command *command){
	int num=command->num_sub_commands;
	int size=0;
	while(command->sub_commands[num-1].argv[size]!=NULL){
		size++;
	}

	command->stdin_redirect=NULL;
	command->stdout_redirect=NULL;
	command->background=0;

	int i;
	for(i=size;i>=0;i--){
		if(command->sub_commands[num-1].argv[i]==NULL) continue;
		if(strcmp("&",command->sub_commands[num-1].argv[i])==0){
			command->background=1;
			command->sub_commands[num-1].argv[i]=NULL;
		}

		else if(strcmp(command->sub_commands[num-1].argv[i],"<")==0){
			command->stdin_redirect=command->sub_commands[num-1].argv[i+1];
			command->sub_commands[num-1].argv[i]=NULL;
			command->sub_commands[num-1].argv[i+1]=NULL;
		}
		else if(strcmp(command->sub_commands[num-1].argv[i],">")==0){
			command->stdout_redirect=command->sub_commands[num-1].argv[i+1];
			command->sub_commands[num-1].argv[i]=NULL;
			command->sub_commands[num-1].argv[i+1]=NULL;
		}

	}
}


void executeSingleCommand(struct Command *command){
	int pid=fork();
	
	if(pid<0){
		fprintf(stderr, "fork failed\n");
		exit(1);
	}
	else if(pid==0){
		
		if(command->stdin_redirect!=NULL){
			close(0);	
			int fd=open(command->stdin_redirect,O_RDONLY);
			if(fd<0){
				//perror(command->stdin_redirect);
				fprintf(stderr,"%s: File not found\n",command->stdin_redirect);
				exit(1);
			}
		}
		if(command->stdout_redirect!=NULL){
			close(1);
			int fd=open(command->stdout_redirect,O_WRONLY | O_CREAT, 0660);
			if(fd<0){
			//	perror(command->stdout_redirect);
				fprintf(stderr,"%s: Cannot create file\n",command->stdout_redirect);
				exit(1);
			}
		}
		execvp(command->sub_commands[0].argv[0],command->sub_commands[0].argv);
		fprintf(stderr,"%s: Command not found\n",command->sub_commands[0].argv[0]);
		exit(1);
	}
	//parent process will print pid if command launched in background
	else{
		int status;
		if(command->background==1){
			 printf("[%d]\n",pid);
		}
		else{
			waitpid(pid,&status,0);
		}
	}

	return;
	


}

void executeCommands(struct Command *command){
	
	//save init input/output
	int tempin=dup(0);
	int tempout=dup(1);

	int in,out;
	//check for input redirect
	if(command->stdin_redirect!=NULL){
		
		int fd=open(command->stdin_redirect,O_RDONLY);
		if(fd<0){
			//perror(command->stdin_redirect);
			fprintf(stderr,"%s: File not found\n",command->stdin_redirect);
			exit(1);				
		}else{
			in=fd;
		}
		
	}else{
		//use default input
		in=dup(tempin);
	}
	
	int pid;
	int i;
	for(i=0;i<command->num_sub_commands;i++){
		//redirect input		
		dup2(in,0);
		close(in);

		//if it's the last sub command, redirect output.
		if(i==command->num_sub_commands-1){
			 if(command->stdout_redirect!=NULL){
				int fd=open(command->stdout_redirect,O_WRONLY | O_CREAT, 0660);
				if(fd<0){
					 //perror(command->stdout_redirect);
					 fprintf(stderr,"%s: Cannot create file\n",command->stdout_redirect);
					 exit(1);

				}else{
					out=fd;
				}
			}else{
				//use default output
				out=dup(tempout);
			}			
		}else{
		//if it's not the last command, create pipe
			int fds[2];
			pipe(fds);
			in=fds[0];
			out=fds[1];

		}
		//redirect output
		dup2(out,1);
		close(out);

		//spawn child
		pid=fork();
		
		if(pid<0){
			perror("Fork failed");
		}
		else if(pid==0){
			execvp(command->sub_commands[i].argv[0],command->sub_commands[i].argv);
			fprintf(stderr,"%s: Command not found\n",command->sub_commands[i].argv[0]);
			exit(1);
		}
			
	}
		
	dup2(tempin,0);
	dup2(tempout,1);
	close(tempin);
	close(tempout);
	int status;
	if(command->background==1){
		//print pid of last sub command
		printf("[%d]\n",pid);
	}else{
		//wait for last sub command		
		int w=waitpid(pid,&status,0);
	}
	
	return;
}

int main(){
	char s[200];
	struct Command command;
	
	while(1){
		printf("$ ");
		fflush(0);
		fgets(s,sizeof s,stdin);
		int length=strlen(s);	
		s[length-1]='\0';
		ReadCommand(s,&command);
		ReadRedirectsAndBackground(&command);
		
		//exit 
		if(strcmp(command.sub_commands[0].argv[0],"exit")==0){
			printf("exit shell\n");
			exit(0);
		}
		//if there are no pipes
		if(command.num_sub_commands==1){
			executeSingleCommand(&command);	
		//if there are pipes
		}else{
			executeCommands(&command);		
		}
		
	}
		
	return 0;
}
