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
	char attr1;
	int attrLength1;
	char attr2;
	int attrLength2;
} Open_File;

typedef struct Scan{
    int fileDesc;
    int op;
    void * value;
    void * current;
} Scan;

/****************************************************
                ~~Global Variables~~
****************************************************/
int openfiles;
int openscans;
Open_File open_files[MAXFILES];
Scan scans[MAXSCANS];
BF_Block * block;

/****************************************************
                ~~Assistant Functions~~
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


int AM_OpenIndex (char *fileName) {

	int fd;
	checkBF(BF_OpenFile(fileName,&fd));
	checkBF(BF_GetBlock(fd,0,block));

	char *data = BF_Block_GetData(block);

	if(memcmp(data,"AM_Index",sizeof("AM_Index")) != 0){
		printf("Error! Wrong type of file\n");
		return -1;
	}


	// Add the opened file to the array
	Open_File file;
	strcpy(file.filename,fileName);
	file.fileDesc = fd;

	data += sizeof("AM_Index");
	memcpy(&file.attr1,data,sizeof(char));
	data += sizeof(char);
	memcpy(&file.attrLength1,data,sizeof(int));
	data += sizeof(int);
	memcpy(&file.attr2,data,sizeof(char));
	data += sizeof(char);
	memcpy(&file.attrLength2,data,sizeof(int));

	int index = hashfile(fd);
	open_files[index] = file;

	// Tests to check the function
	printf("FileName: %s\n",open_files[index].filename);
	printf("FileDesc: %d\n",open_files[index].fileDesc);
	printf("Attr1: %c\n",open_files[index].attr1);
	printf("AttrLength1: %d\n",open_files[index].attrLength1);
	printf("Attr2: %c\n",open_files[index].attr2);
	printf("attrLength2: %d\n",open_files[index].attrLength2);

  	BF_UnpinBlock(block);

  return AME_OK;
}


int AM_CloseIndex (int fileDesc) {

  	// Locate the position of the file
	int index = hashfile(fileDesc);

	if(open_files[index].fileDesc == -1){
		printf("Error! An open version of the file");
		printf(" doesn't exist\n");
		return AME_FILEEXISTS;
	}

	// Check whether there are any open scans
	for(int i = 0; i < MAXSCANS; i++){
		if(scans[index].fileDesc != -1){
			printf("Error! There are open scans for the file\n");
			return AME_FILEEXISTS;
		}
	}

	/* Everything's fine,
		remove the file */
	open_files[index].fileDesc = -1;

  	return AME_OK;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {

	int index = hashfile(fileDesc);
	int blocks_n;
	char *data;
	int counter;
	int null_pointer = -1;
	checkBF(BF_GetBlockCounter(fileDesc , &blocks_n));

	if (blocks_n == 1)
	{
		//ftiaxnoume to prwto block eurethriou
		checkBF(BF_AllocateBlock(fileDesc , block));
		data = BF_Block_GetData(block);
		 // E gia eurethrio , einani to anagnwristiko oti prokeitai gia block eurethriou kai oxi otidhpote allo
		memcpy(data , "E" ,sizeof("E"));
		data += sizeof("E");
		counter = 1;
		memcpy (data , &counter , sizeof(int));
		data += sizeof(int);
		memcpy(data ,&null_pointer , sizeof(int));
		data += sizeof(int);
		memcpy(data , value1 ,open_files[index].attrLength1);				//
		data += open_files[index].attrLength1;
		memcpy(data , &(blocks_n+1) , sizeof(int));

		//ftiaxnoume to prwto block dedomenwn
		checkBF(BF_AllocateBlock(fileDesc , block));
		data = BF_Block_GetData(block);
		// D gia dedomena , einai to anagnwristiko oti prokeitai gia block dedomenwn kai oxi otidhpote allo
		memcpy(data , "D" , sizeof("D"));
		data += sizeof("D");
		memcpy(data , &null_pointer , sizeof(int));
		data += sizeof(int);
		memcpy(data , &counter , sizeof(int));
		data += sizeof(int);
		memcpy(data , value1 , open_files[index].attrLength1);
		data += open_files[index].attrLength1;
		memcpy(data , value2 , open_files[index].attrLength2);


	}


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
