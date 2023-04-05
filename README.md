# CS:APP lab
> CS:APP记录
## Table of Contents
* [ShellLab](#shelllab)
<!-- * [License](#license) -->

## ShellLab
该项目是基于C语言编写一个的支持基本命令、I/O重定向、管道、信号和进程控制的Unix shell

lab原始代码链接：
[http://csapp.cs.cmu.edu/3e/shlab-handout.tar](http://csapp.cs.cmu.edu/3e/shlab-handout.tar)

lab的pdf链接：
[http://csapp.cs.cmu.edu/3e/shlab.pdf](http://csapp.cs.cmu.edu/3e/shlab.pdf)

### 一、作业解释和要求
这个Lab主要要求实现一个最简单的Unix shell程序（貌似是tinyshell）
需要具体实现以下函数：

 - eval：主程序，用于分析、解释命令行
 - builtin_cmd：识别、解释 bulit-in 命令：如quit，fg，bg 和jobs
 - do_bgfg：执行 bg 和 fg 这些bulit-in命令
 - waitfg：等待前台程序（foreground job）完成
 - sigchld_handler：获取 SIGCHILD 信号
 - sigint_handler：获取 SIGINT（ctrl-c）信号
 - sigtstp_handler：获取SIGTSTP（ctrl-z） 信号
 
 ### 二、 Unix Shells的大致介绍
 Shell是一个交互的命令行解释器，并且其在用户面前运行程序。一个shell反复地打印提示，等待stdin的命令行（command line），随后按照命令行执行相应的操作。
命令行是一个以空格分隔的ASCII文本单词序列。命令行中的第一个单词是内置命令的名称或可执行文件的路径名。剩下的单词是命令行参数。如果第一个单词是内置命令，shell会立即在当前进程中执行该命令。否则，该字被假定为可执行程序的路径名。在这种情况下，shell会fork一个子进程，然后在该子进程的上下文中加载并运行该程序。由于解释单个命令行而创建的子进程统称为作业。通常，一个作业可以由多个通过Unix管道连接的子进程组成。

例如输入命令行：
```c
tsh> jobs
```
这使得 shell 执行 built-in 中的 jobs 指令。
输入命令行：
```bash
tsh> /bin/ls -l -d
```
使得在前台运行ls程序。按照约定，shell确保当程序开始执行它的主例程时

```bash
int main(int argc, char *argv[])
```
其中 args 和 argv 参数有如下的值：

```bash
argc == 3
argv[0] == ‘‘/bin/ls’’
argv[1]== ‘‘-l’’
argv[2]== ‘‘-d’’
```
或者，键入命令行：

```bash
tsh> /bin/ls -l -d 
```
使得ls程序运行在后台。

Shell支持作业控制（jobs contorl）的概念，它允许用户在后台和前台之间来回移动作业，并更改作业中进程的状态(运行、停止或终止)。键入 ctrl-c 将导致一个 SIGINT 信号传递给前台作业中的每个进程。SIGINT 的默认操作是终止进程。类似地，键入 ctrl-z 将导致向前台作业中的每个进程发送 SIGTSTP 信号。SIGTSTP的默认操作是将进程置于停止状态，直到被接收方唤醒为止。比如：

 - jobs：列出正在运行和已停止的后台任务
 - bg<job>：将已停止的后台作业更改为正在运行的后台作业。
 - fg<job>：将已停止或正在运行的后台作业更改为正在前台运行的作业。
 - kill<job>：杀死一个作业（进程）

### 三、代码
#### 0. main主程序

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

#### 1.eval

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

#### 2.builtin_cmd

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

## 3. do_bgfg

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

#### 4.waitfg

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

#### 5.sigchld_handler

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

#### 6.sigint_handler

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

#### 7.sigtstp_handler

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
