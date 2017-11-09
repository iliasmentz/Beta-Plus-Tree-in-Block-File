#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AM.h"
#include "bf.h"
/****************************************************
                ~~Struct Definitions~~
****************************************************/

typedef struct file{
    char filename[128];
    int fd;
    char keytype;
    char datatype;
    /*node * tree;*/
} file;



/****************************************************
                ~~Global Variables~~
****************************************************/
int openfiles;
int openscans;
BF_Block * block;


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

/****************************************************
                ~~AM FUNCTIONS~~
****************************************************/

int AM_errno = AME_OK;

void AM_Init() {
    BF_Init(LRU);
    BF_Block_Init(&block);
    openfiles =0;
    openscans =0;
	return;
}


int AM_CreateIndex(char *fileName, char attrType1, int attrLength1, char attrType2, int attrLength2)
{
    /*check if the lengths for int and float are valid*/
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
   data += sizeof(char);

   // memcpy(data, &attrLength1, sizeof(int));
   sprintf(data, "%d",attrLength1);
   data += sizeof(int);

   (*data) = attrType2;
   data += sizeof(char);

   // memcpy(data, &attrLength2, sizeof(int));
   sprintf(data, "%d",attrLength2);
   data += sizeof(int);

   int root=0;
   // memcpy(data, &root, sizeof(int));
   sprintf(data, "%d",root);
   BF_Block_SetDirty(block);
   BF_UnpinBlock(block);
   return AME_OK;
}


int AM_DestroyIndex(char *fileName) {
  return AME_OK;
}


int AM_OpenIndex (char *fileName) {
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
