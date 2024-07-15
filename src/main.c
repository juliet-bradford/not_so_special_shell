
/* libraries */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "fields.h"
#include "jval.h"
#include "jrb.h"


int main(int argc, char** argv) {

	/* initializations */

	char *prompt, *fout, *fin; 
	int status, pid, ampersand, append, inpipe, skip, numcommands, curr_command, curr_argc,i, j;
	IS is;
	JRB pids, tmp;

	int pipefd[2], prevpipefd[2];

	/* connect the input stream to stdin and error check */

	is = new_inputstruct(NULL);
	if (is==NULL) {
		fprintf(stderr, "Error opening stdin\n");
		exit(1);
	}


	/* command line argument parsing */

	/* optional second argument used as shell prompt */ 
	/* if not specified "jsh: " is used              */

	if (argc==1) prompt = "jsh: ";
	else if (argc==2) prompt = argv[1];
	else {
		fprintf(stderr, "Usage: ./main [optional prompt]\n");
		exit(1);
	}


	/* if our prompt is "-" we use no prompt */

	if (strcmp(prompt, "-") == 0) prompt = "";
	

	/* print prompt */

	printf("%s", prompt);


	/* while input steam has not reached eof */

	while (get_line(is) >= 0) {

		/* skip past empty lines */

		if (is->NF==0) { printf("%s", prompt); continue; }


		/* returns from program on exit */

		if (is->NF==1 && strcmp(is->fields[0], "exit") == 0) break;
		

		/* resetting all of our resources for next line parsing */

		numcommands = 1;
		curr_argc = 0;
		curr_command = 0;
		ampersand = 0;
		append = 0;
		inpipe = 0;
		skip = 0;
		fout = NULL;
		fin = NULL;


		/* get the number of piped commands and make double pointer char array of that size */

		for (i=0; i<is->NF; ++i)
			if (strcmp(is->fields[i], "|") == 0)
				++numcommands;


		char **newargvs[numcommands];


		/* while we can still parse a line element */

		for (i=0; i<is->NF; ++i) {

			/* if we are reading the process call */
			
			if (curr_argc == 0) {

				/* while we are still reading our process call */

				int start = i;
				while ( i!=is->NF
					&& strcmp(is->fields[i], "&" ) != 0
					&& strcmp(is->fields[i], ">" ) != 0
					&& strcmp(is->fields[i], ">>") != 0
					&& strcmp(is->fields[i], "<" ) != 0
					&& strcmp(is->fields[i], "|" ) != 0) {

					/* incriment curr_argc and i */

					++curr_argc;
					++i;
				}


				/* incriments curr_argc one last time and decriments i for next loop iteration */

				++curr_argc;
				--i;


				/* allocate memory for arguments of current command and set the arguments */

				newargvs[curr_command] = malloc(sizeof(char *) * curr_argc);
				for (j=start; j-start<curr_argc-1; ++j)
					newargvs[curr_command][j-start] = is->fields[j];
				newargvs[curr_command][curr_argc-1] = NULL;
			}


			/* if we are reading a piping */

			else if (strcmp(is->fields[i], "|") == 0) {

				/* set inpipe for error checking, reset argc counter, incriment current command counter */

				inpipe = 1;
				curr_argc = 0;
				++curr_command;
			}


			/* if we are reading an ampersand */

			else if (strcmp(is->fields[i], "&") == 0) {
			
				/* ampersand is only valid at the end of the command */

				if (i!=is->NF-1) {
					fprintf(stderr, "Error: Ampersand only allowed at the end of the command\n");
					exit(1);
				}


				/* set the ampersand boolean */

				ampersand = 1;
			}
			

			/* if we are reading output redirection (truncation) */

			else if (strcmp(is->fields[i], ">") == 0) {
			
				/* check for bad output redirection */

				if (curr_command!=numcommands-1 || fout!=NULL) {
					fprintf(stderr, "Error: Ambigous output redirection\n");
					skip = 1;
					break;
				}


				/* incriment to the value of the file and error check for bad usage */

				++i;

				if (is->NF==i) {
					fprintf(stderr, "ERROR: no output file specified\n");
					skip = 1;
					break;
				}


				/* set the output file to the specified file */

				fout = is->fields[i];

			}


			/* if we are reading output redirection (appending) */

			else if (strcmp(is->fields[i], ">>") == 0) {

				/* check for bad output redirection */

				if (curr_command!=numcommands-1 || fout!=NULL) {
					fprintf(stderr, "Error: Ambigous output redirection\n");
					skip = 1;
					break;
				}


				/* incriment to the value of the file and error check for bad usage */

				++i;
				
				if (is->NF==i) {
					fprintf(stderr, "ERROR: no append file specified\n");
					skip = 1;
					break;
				}


				/* set the output file to the specified file and mark for appending */

				fout = is->fields[i];
				append = 1;

			}


			/* if we are reading input redirection */

			else if (strcmp(is->fields[i], "<") == 0) {

				/* check for bad input redirection */

				if (inpipe) {
					fprintf(stderr, "Error: Ambigous input redirection\n");
					skip = 1;
					break;
				}


				/* incriment to the value of the file */

				++i;


				/* error checking for bad usage */

				if (is->NF==i) {
					fprintf(stderr, "ERROR: no input file specified\n");
					skip = 1;
					break;
				}


				/* set the input file tot the specified file */

				fin = is->fields[i];
			}
		}


		/* if we are given bad input, we don't exit, we just print our shell and continue */

		if (skip) { printf("%s", prompt); continue; }
		
	
		/* initialize our pid tree */
	
		pids = make_jrb();


		/* for every command in our pipe */

		for (i=0; i<numcommands; ++i) {

			/* if we still commands to pipe output to */

			if (i!=numcommands-1) {

				/* set up pipe */

				j = pipe(pipefd);
				if (j < 0) {
					perror("pipe(pipefd)");
					exit(1);
				}
			}


			/* fork process */
		
			if ((pid = fork()) == 0) {

				/* check for input specifications */

				if (i == 0 && fin != NULL) {

					int fdin; 

					/* open file for reading and error check */

					fdin = open(fin, O_RDONLY);
					if (fdin < 0) {
						perror("open");
						exit(1);
					}


					/* redirect stdin to read from the file and close the file */

					if (dup2(fdin, 0) != 0) {
						perror("dup2(fdin, 0)");
						exit(2);
					}
					
					close(fdin);
				
				}


				/*if we have a previous process in our pipe */

				if (i != 0) {
				
					/* pipe input from previous process */

					if (dup2(prevpipefd[0], 0) != 0) {
						perror("dup2(prevpipefd[0], 0)");
						exit(1);
					}
				}


				/* if this is the last command in our pipe */

				if (i == numcommands-1) {
			
					int fdout; 

					/* and we have a file to pipe our output to */

					if (fout != NULL) {
				
						/* if that file is specified for appending */

						if (append) {

							/* open file for writing and appending and error check */

							fdout = open(fout, O_WRONLY | O_APPEND | O_CREAT, 0644);
							if (fdout < 0) {
								perror("open");
								exit(2);
							}


							/* redirect stdout to write to the file (appending it) */

							if (dup2(fdout, 1) != 1) {
								perror("dup2(fdout, 1)");
								exit(1);
							}
						}


						/* otherwise, file should be truncated */

						else {

							/* open file for writing and truncation and error check */

							fdout = open(fout, O_WRONLY | O_TRUNC | O_CREAT, 0644);
							if (fdout < 0) {
								perror("open");
								exit(2);
							}


							/* redirect stdout to write to the file (overwritting it) */

							if (dup2(fdout, 1) != 1) {
								perror("dup2(fdout, 1)");
								exit(1);
							}
						}

						/* close file descriptor */

						close(fdout);

					}
				} 
				

				/* if we aren't at the last process, we need to pipe output for next process */

				else {

					/* pipe output for next process */

					if (dup2(pipefd[1], 1) != 1) {
						perror("dup2(pipefd[1], 1)");
						exit(1);
					}
				}

				
				/* close file descriptors */

				/* all pipefds are defined for all processes other than the first */

				if (i!=0) {
					close(pipefd[0]);
					close(pipefd[1]);
					close(prevpipefd[0]);
					close(prevpipefd[1]);
				}


				/* if we only have 1 process, the first process has no pipefds defined */

				else if (numcommands!=1) {
					close(pipefd[0]);
					close(pipefd[1]);
				}


				/* execution call */

				execvp(newargvs[i][0], newargvs[i]);

				
				/* if we return then something went wrong */
			
				perror(newargvs[i][0]);
				exit(1);

			} 
			

			/* we are in the parent process */

			else {

				/* add pid to jrb tree */

				jrb_insert_int(pids, pid, new_jval_i(pid));


				/* close unused pipes and track the now previous pipes */

				if (i!=0) {
					close(prevpipefd[0]);
					close(prevpipefd[1]);
				}

				prevpipefd[0] = pipefd[0];
				prevpipefd[1] = pipefd[1];
			}
		}
	

		/* if an ampersand wasn't specified, we need to wait for processes in the pipe to terminate */

		if (!ampersand) {

			/* we need to wait for child processes */

			while (!jrb_empty(pids)) {

				/* we can assume all signals indicate termination */

				pid = wait(&status);


				/* find pid in our tree and free it */

				tmp = jrb_find_int(pids, pid);
				if (tmp!=NULL) 
					jrb_delete_node(tmp);
			}
		}


		/* free our tree and all command line arguments */

		jrb_free_tree(pids);
	
		for (i=0; i<numcommands; ++i)
			free(newargvs[i]);


		/* display prompt to user */

		printf("%s", prompt);

	}



	/* clear out all zombie process */

	while(wait(&status) != -1) ;
	

	/* free the input stuct */

	jettison_inputstruct(is);

	return 0;
}
