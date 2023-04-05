# CS:APP lab
> CS:APP记录
## Table of Contents
* [ShellLab](#shelllab)
<!-- * [License](#license) -->

## ShellLab
The project is to write a Unix shell in C language that supports basic commands, I/O redirection, pipes, signals, and process control.

Original code link：
[http://csapp.cs.cmu.edu/3e/shlab-handout.tar](http://csapp.cs.cmu.edu/3e/shlab-handout.tar)

PDF link：
[http://csapp.cs.cmu.edu/3e/shlab.pdf](http://csapp.cs.cmu.edu/3e/shlab.pdf)
 ### 1. Introduce of Unix Shells
A shell is an interactive command-line interpreter that runs programs on behalf of the user. A shell repeatedly prints a prompt, waits for a command line on stdin, and then carries out some action, as directed by the contents of the command line. 
 
The command line is a sequence of ASCII text words delimited by whitespace. The first word in the command line is either the name of a built-in command or the pathname of an executable file. The remaining words are command-line arguments. If the first word is a built-in command, the shell immediately executes the command in the current process. Otherwise, the word is assumed to be the pathname of an executable program. In this case, the shell forks a child process, then loads and runs the program in the context of the child. The child processes created as a result of interpreting a single command line are known collectively as a job. In general, a job can consist of multiple child processes connected by Unix pipes.

### 2. Explanation and Requirements 
This lab requires the implementation of a simple Unix shell program (tinyshell) that specifically needs to implement the following functions.

 - eval：The main program that parsing and interpreting the command line.
 - builtin_cmd：recognize and interpret bulit-in commands, such as quit，fg，bg and jobs
 - do_bgfg：execute built-in commands such as bg and fg
 - waitfg：wait for foreground jobs to complete
 - sigchld_handler：get the SIGCHILD signal
 - sigint_handler：get the SIGINT (ctrl-c) signal
 - sigtstp_handler：get the SIGTSTP (ctrl-z) signal
 


### 3.Code
#### 3.1 main()

```c

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
```

#### 3.2 eval()

```c
void eval(char *cmdline) 
{
    char *argv[MAXARGS];    /* Argument list execve() */
    char buf[MAXLINE];      /* Holds modified command line */
    int bg;                 /* Should the job run in bg or fg */
    pid_t pid;              /* Process id */

    sigset_t mask_all, mask_one, prev_one;

    sigfillset(&mask_all);
    sigemptyset(&mask_one);
    sigaddset(&mask_one,SIGCHLD);

    strcpy(buf,cmdline);
    bg = parseline(buf, argv);
    if(argv[0] == NULL)
        return; /* Ignore empty lines*/

    if(!builtin_cmd(argv)){
        sigprocmask(SIG_BLOCK, &mask_one, &prev_one);   /*Block SIGCHLD*/
        if((pid = fork()) == 0){    /* Child runs user job*/
            setpgid(0, 0);
            sigprocmask(SIG_SETMASK, &prev_one, NULL);  /* Unblock SIGCHLD*/
            if(execve(argv[0], argv,environ) < 0){
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }           
        }
        sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Parent process*/
        
        /* Parent waits for foreground job to terminate*/
        if(!bg){    /* foreground */
            addjob(jobs,pid,FG,cmdline);/* add jobs */
            sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD*/  
            waitfg(pid);
            /*int status;
            if(waitpid(pid, &status, 0)<0)
                unix_error("waitfg:wait error");
            */
        }
        else{   /* background */   
            addjob(jobs,pid,BG,cmdline);/* add jobs */
            sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD*/         
            printf("[%d] (%d) %s", pid2jid(pid),pid,cmdline);
        }
            
    }
    return;
}
```

#### 3.3 builtin_cmd

```c
/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    if(!strcmp(argv[0],"quit")) /* quit command*/
        exit(0);
    if(!strcmp(argv[0],"fg")){ /* fg command*/
        do_bgfg(argv);
        return 1;
    }
    if(!strcmp(argv[0],"bg")){ /* bg command*/
        do_bgfg(argv);
        return 1;
    }
    if(!strcmp(argv[0],"jobs")){ /* jobs command*/
        listjobs(jobs);
        return 1;
    }
    if(!strcmp(argv[0],"kill")){ /* jobs command*/
        //printf(argv[1]);
        //Kill(,SIGKILL);
        //exit(0);
        return 1;
    }
    return 0;     /* not a builtin command */
}
```

<hr style=" border:solid; width:100px; height:1px;" color=#000000 size=1">

## 3.4 do_bgfg

```c
/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    if(argv[1]==NULL){
        printf("%s command requires PID or %%jobid argument\n",argv[0]);
        return;
    }
    if ((argv[1][0] < '0' || argv[1][0] > '9') && argv[1][0] != '%') {
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }
    if(!strcmp(argv[0],"fg")){  /* frontground */

        int len = strlen(argv[1]);
        pid_t pid;
        struct job_t *job_ptr;
        if(argv[1][0]=='%'){    //jid
            int jid=atoi(argv[1]+1);
            //printf("jid:%d \n",jid);fflush(stdout);
            job_ptr = getjobjid(jobs, jid);
            if(job_ptr == NULL){
                printf("%%%d: No such job\n",jid);
                return;
            }
            else{
                pid = job_ptr->pid;
            }
            
            //printf("sent SIGCONT to job %%%d",job_ptr->pid);
        }
        else{   //pid 
            pid=atoi(argv[1]);
            job_ptr = getjobpid(jobs, pid);
            if(job_ptr==NULL){
                printf("(%d): No such process\n",pid);
                return;
            }
        }
        kill(-pid,SIGCONT);
        job_ptr->state = FG;
        waitfg(pid); 
        
    }
    else if(!strcmp(argv[0],"bg")){ /* background */
        //
        int len = strlen(argv[1]);
        pid_t pid;
        struct job_t *job_ptr;
        if(argv[1][0]=='%'){    /* jid */
            int jid=atoi(argv[1]+1);
            //printf("jid:%d \n",jid);fflush(stdout);
            job_ptr = getjobjid(jobs, jid);
            if(job_ptr == NULL){
                printf("%%%d: No such job\n",jid);
                return;
            }
            else{
                pid = job_ptr->pid;
            }
            //printf("sent SIGCONT to job %%%d",job_ptr->pid);
        }
        else{   /* pid */
            pid=atoi(argv[1]);
            job_ptr = getjobpid(jobs, pid);
            if(job_ptr==NULL){
                printf("(%d): No such process\n",pid);
                return;
            }
            //printf("jid:%d\n",jid);fflush(stdout);
        }
        kill(-pid,SIGCONT);
        job_ptr->state = BG;
        printf("[%d] (%d) %s",job_ptr->jid,job_ptr->pid,job_ptr->cmdline);
    }
    return;
}

```

#### 3.5 waitfg

```c
/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{   //in pdf, recommand to use sleep()
    //sigset_t prev_one;
    //sigemptyset(&prev_one);
    //sigprocmask(SIG_SETMASK, &prev_one, NULL);
    while (pid == fgpid(jobs)) {
        //sigsuspend(&prev_one);
        //printf("1");
        sleep(1);
    }
    // eval 中父进程阻塞了所有信号，在这里释放
    
    return;
}

```

#### 3.6 sigchld_handler

```c
/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{   //use waitpid()
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    sigfillset(&mask_all);
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED))>0){ /* Reap a zombie child*/
        
        if(WIFEXITED(status)){
            //struct job_t *job_ptr = getjobpid(jobs, pid);
            //printf("Job [%d] (%d) nomorly terminated by signal %d\n",job_ptr->jid,job_ptr->pid,WTERMSIG(status));
            //printf("Job [%d] (%d) successful exit!\n",job_ptr->jid,job_ptr->pid);
            sigprocmask(SIG_BLOCK,&mask_all,&prev_all);
            deletejob(jobs,pid);
            sigprocmask(SIG_SETMASK,&prev_all, NULL);
        }  
        else if (WIFSIGNALED(status)){
            //terminated
            struct job_t *job_ptr = getjobpid(jobs, pid);
            printf("Job [%d] (%d) terminated by signal %d\n",job_ptr->jid,job_ptr->pid,WTERMSIG(status));
            sigprocmask(SIG_BLOCK,&mask_all,&prev_all);
            deletejob(jobs,pid);
            sigprocmask(SIG_SETMASK,&prev_all, NULL);
        }
        else if(WIFSTOPPED(status)){
            //stop
            struct job_t *job_ptr = getjobpid(jobs, pid);
            printf("Job [%d] (%d) stopped by signal %d\n",job_ptr->jid,job_ptr->pid,WTERMSIG(status));

            //sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            job_ptr->state = ST;//更改当前job的状态
            //sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
    }
    
    errno = olderrno;
    return;
}

```

#### 3.7 sigint_handler

```c
/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    int old_errno = errno;
    //printf("Caught SIGINT!\n");fflush(stdout);
    pid_t curr_fgpid=fgpid(jobs);
    if(curr_fgpid!=0){
        kill(-curr_fgpid,sig);
    }
    errno = old_errno;
    //return;
}

```

#### 3.8 sigtstp_handler

```c
/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    int old_errno = errno;
    //printf("Caught SIGSTP!\n");fflush(stdout);
    pid_t curr_fgpid=fgpid(jobs);
    if(curr_fgpid!=0){
        kill(-curr_fgpid,sig);
    }
    
    errno = old_errno;
    //return;
}
```
