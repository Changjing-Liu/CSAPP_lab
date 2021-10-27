#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

struct Line{
    int valid;
    int tag;
    int LRUcounter;
};

struct Set{
    struct Line* lines;
};

struct Cache{
    int s;
    int E;
    int b;
    struct Set* sets;
};

struct Result{
    int hit;
    int miss;
    int eviction; 
};
struct Result TestHit(struct Set TargetSet,int tag,int E,struct Result results,int flag,int flag2){
    int hitflag=0;
    for(int i=0;i<E;i++){
        //hit
        if(TargetSet.lines[i].tag==tag&&TargetSet.lines[i].valid==1){
            if(flag) printf("hit ");
            hitflag=1;
            results.hit++;//hit++
            TargetSet.lines[i].LRUcounter++;
            break;
        }
    }
    if(!hitflag){
        //not hit
        if(flag) printf("miss ");
        results.miss++;//miss++
        int MaxCounter=TargetSet.lines[0].LRUcounter;
        int MinCounter=TargetSet.lines[0].LRUcounter;
        int LineIndex=0;
        for(int i=0;i<E;i++){
            //find the max/min
            if(TargetSet.lines[i].LRUcounter>MaxCounter)
                MaxCounter=TargetSet.lines[i].LRUcounter;
            if(TargetSet.lines[i].LRUcounter<MinCounter){
                MinCounter=TargetSet.lines[i].LRUcounter;
                LineIndex=i;//find the index of the Least Recently Used
            }
        }
        //replace
        TargetSet.lines[LineIndex].LRUcounter=MaxCounter+1;
        TargetSet.lines[LineIndex].tag=tag;
        //if valid
        if(TargetSet.lines[LineIndex].valid){
            if(flag) printf("eviction ");
            results.eviction++;//evit++
        }
        else TargetSet.lines[LineIndex].valid=1;
    }
    if(flag&&flag2==0) printf("\n");
    return results;
};

struct Result Test(FILE* fp,struct Cache cache,int flag){
    struct Result results={0,0,0};
    //format like this I 0400D7D4,8
    char ch;//Load，store，modify
    long unsigned int address; //address
    int size,b=cache.b,s=cache.s,E=cache.E,S=1<<s;
    while((fscanf(fp,"%c %lx,%d[^\n]",&ch,&address,&size))!=-1){
        if(ch=='I') continue;
        else{
            //address ：t(tag) s(set index) b(block offset(not used))
            int set_index=(address>>b)&(S-1);//shift right b bit,and & S-1(0x1000000-0x1=0x111111)
            int tag=(address>>b)>>s;
            struct Set TargetSet=cache.sets[set_index];
            if(ch=='L'||ch=='S'){
                if(flag) printf("%c %lx %d",ch,address,size);
                results=TestHit(TargetSet,tag,E,results,flag,0);
            }
            else if(ch=='M'){
            if(flag) printf("%c %lx %d",ch,address,size);
            results=TestHit(TargetSet,tag,E,results,flag,1);
            results=TestHit(TargetSet,tag,E,results,flag,0);
            }
        else continue;
        }      
    }
    return results;
};

struct Cache InitializeCache(int s,int E,int b){
    struct Cache New_Cache;
    int S=0;
    New_Cache.s=s;
    New_Cache.E=E;
    New_Cache.b=b;
    S=1<<s;
    if((New_Cache.sets=(struct Set*)malloc(S*sizeof(struct Set)))==NULL){
        perror("Failed to create sets in the cache");
    }
    for(int i=0;i<S;i++){
        if((New_Cache.sets[i].lines=(struct Line *)malloc(E*sizeof(struct Line)))==NULL){
            perror("Failed to create lines in sets");
        }
    }
    return New_Cache;
};

void Release(struct Cache cache){
    int S=1<<cache.s;
    for(int i=0;i<S;i++){
        free(cache.sets[i].lines);
    }
}

int main(int argc, char *argv[])
{
    struct Result Results={0,0,0};
    FILE* fp=NULL;
    struct Cache Sim_Cache={0,0,0,NULL};
    int s=0,b=0,E=0,flag=0;
    const char *command_options="hvs:E:b:t:";
    char ch;
    while((ch=getopt(argc,argv,command_options))!=-1){
        switch(ch){
            case'h':
                exit(EXIT_SUCCESS);
            case 'v':
                flag=1;
                break;
            case 's':
                s=atol(optarg);
                break;
            case 'E':
                E=atol(optarg);
                break;
            case 'b':
                b=atol(optarg);
                break;
            case 't':
                if((fp=fopen((const char *)optarg,"r"))==NULL){
                    perror("failed to open tracefile!");
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
    if(s==0||E==0||b==0||fp==NULL){
        printf("fail");
        exit(EXIT_FAILURE);
    }
    Sim_Cache=InitializeCache(s,E,b);
    Results=Test(fp,Sim_Cache,flag);
    printSummary(Results.hit,Results.miss,Results.eviction);
    Release(Sim_Cache);
    return 0;
}
