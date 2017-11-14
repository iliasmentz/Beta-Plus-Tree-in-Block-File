#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AM.h"
#include "bf.h"
/****************************************************
                ~~Struct Definitions~~
****************************************************/

typedef struct open_file{
    char filename[60];
    int fileDesc;
    char keytype;
    char datatype;
  } Open_File;

typedef struct filesArray {
  char attr1;
  int attrLength1;
  char attr2;
  int attrLength2;
  int file_d;
} filesArray;


typedef struct Scan{
    int fileDesc;
    int op;
    void * value;
    void * current;
} Scan;

typedef struct stack_node {
    int fd;
    struct stack_node * next;
}stack_node;

typedef struct stack{
    stack_node * start;
    stack_node * end;
    int size;
}stack;

stack * create_stack()
{
    stack * s = malloc(sizeof(stack));
    stack->start = NULL;
    stack->end = NULL;
    stack->size = 0;
    return s;
}

stack_node * create_node(int fd)
{
    stack_node * node = malloc(sizeof(stack_node));
    node->fd = fd;
    node->next = NULL;
    return node;
}

void push(stack * s, int fd)
{
    stack_node * node = create_node(fd);
    if(size==0)
    {
        s->start = node;
        s->end = node;
    }
    else
    {
        s->end->next = node;
        s->end = node;
    }
    s->size ++;
}

int pop(stack *s)
{
    if(s->size ==0)
        return -1;

    
}
/****************************************************
                ~~Global Variables~~
****************************************************/
int openfiles;
int openscans;
Open_File open_files[MAXFILES];
Scan scans[MAXSCANS];
BF_Block * block;
filesArray *open;


/****************************************************
                ~~Assistan Functions~~
****************************************************/
void checkBF(BF_ErrorCode e)
{
    if(e != BF_OK)
    {
        BF_PrintError(e);
        exit(e);
    }
}

/*Upon successful completion, the open function shall open the file and return
a non-negative integer representing the lowest numbered unused file descriptor.
By this way, */
int hashfile(int fd)
{
    return fd%MAXFILES;
}


/****************************************************
                ~~AM FUNCTIONS~~
****************************************************/

int AM_errno = AME_OK;

void AM_Init() {
    BF_Init(LRU);
    BF_Block_Init(&block);
    open = (filesArray *)malloc(MAXFILES *sizeof(filesArray));
    openfiles =0;
    openscans =0;


    int i;
    for ( i=0 ; i<MAXFILES; i++)
        open_files[i].fileDesc = -1;
    for(i=0; i<MAXSCANS; i++)
        scans[i].fileDesc = -1;
	return;
}


int AM_CreateIndex(char *fileName, char attrType1, int attrLength1, char attrType2, int attrLength2)
{
    /*check if the lengths for int and float are valid*/
    printf("%s\n", fileName );
   if ( (attrType1 == 'i' && attrLength1!=sizeof(int)) || (attrType2 == 'i' && attrLength2!=sizeof(int)) ||
   (attrType1 == 'f' && attrLength1!=sizeof(float)) || (attrType2 == 'f' && attrLength2!=sizeof(float)) )
   {
       AM_errno = -1;
       return AM_errno;
   }

   checkBF(BF_CreateFile(fileName));

   int fd;
   checkBF(BF_OpenFile(fileName, &fd));
   checkBF(BF_AllocateBlock(fd, block));

   char * data = BF_Block_GetData(block);
   memcpy(data, "AM_Index", sizeof("AM_Index"));
   data += sizeof("AM_Index");

   (*data) = attrType1;
   // printf("%c\n", *data );
   data += sizeof(char);

    memcpy(data, &attrLength1, sizeof(int));
   // sprintf(data, "%d",attrLength1);
   // printf("INt :%d\n", *(int * )data );
   data += sizeof(int);

   (*data) = attrType2;
   // printf("%c\n", *data );
   data += sizeof(char);


    memcpy(data, &attrLength2, sizeof(int));
    // printf("INt :%d\n", *(int * )data );

    // sprintf(data, "%d",attrLength2);
   data += sizeof(int);

   int root=0;
    memcpy(data, &root, sizeof(int));
  // sprintf(data, "%d",root);
   BF_Block_SetDirty(block);
   BF_UnpinBlock(block);
   BF_CloseFile(fd);
   return AME_OK;
}


int AM_DestroyIndex(char *fileName) {

    int i;
    for(i=0; i<MAXFILES; i++)
    {
        if(open_files[i].fileDesc >= 0)
        {
            if(strcmp(fileName, open_files[i].filename)==0)
            {
                AM_errno = AME_FILEOPEN;
                fprintf(stderr, "Can't Delete! File is open\n");
                return AM_errno;
            }
        }
    }

    int status = remove(fileName);
    if(status ==0)
        printf("File %s deleted", fileName);
    else
    {
            AM_errno = AME_CANTDESTROY;
            fprintf(stderr, "Can't Destroy the file for some reason\n" );
    }
    return AME_OK;
}

  int l =0;
int AM_OpenIndex (char *fileName) {

  int fd;
  char *data;
  char s[100];
  char n[100];

  int num;
  checkBF(BF_OpenFile(fileName , &fd));
  checkBF(BF_GetBlock(fd, 0, block));
  data = BF_Block_GetData(block);

  if (memcmp(data, "AM_Index", sizeof("AM_Index"))!=0)
  {
      printf("ERROR %s\n", fileName );
      return -1;
  }
  data += sizeof("AM_Index");                     //dokimastika
  if (l == 0){
      open[0].attr1 = (*data);
      printf("EDW %s\n", (data));
      data += sizeof(char);
      open[0].attrLength1 = *(int *) data;
    //   sprintf(s, "%d" , data);
    //   num = atoi(s);
    //   printf("%d\n" , num);
      data += sizeof(int);
      open[0].attr2 = (*data);
      data += sizeof(char);
        //   sprintf(n, "%d" ,data);
        //   num = atoi(n);
        //   printf("%d\n",num);
      open[0].attrLength2 = *(int *) data;
      open[0].file_d = fd;
      l =1;
  }
  //sprintf(open[0].file_d , "%d" , &fd);
  printf("%c kai %d kai %c kai %d\n", open[0].attr1 , open[0].attrLength1 , open[0].attr2 , open[0].attrLength2 );

  BF_UnpinBlock(block);



  return AME_OK;
}


int AM_CloseIndex (int fileDesc) {
  return AME_OK;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
  return AME_OK;
}


int AM_OpenIndexScan(int fileDesc, int op, void *value) {
  return AME_OK;
}


void *AM_FindNextEntry(int scanDesc) {

}


int AM_CloseIndexScan(int scanDesc) {
  return AME_OK;
}


void AM_PrintError(char *errString) {

}

void AM_Close() {
    BF_Block_Destroy(&block);
    BF_Close();
}
