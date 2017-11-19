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

typedef struct scan_point{
	void * value1;
	void * value2;
	int block_num;
	int record_num;
	int max_record;
	int next_block;
}scan_point;


typedef struct Scan{
    int fileDesc;
    int op;
    void * value;
    scan_point * current;
} Scan;


/****************************************************
        ~~Our Stack for keeping Parents~~
****************************************************/

/*  Definitions */
typedef struct stack_node {
    int block_num;
    struct stack_node * next;
}stack_node;

typedef struct stack{
    stack_node * start;
    stack_node * end;
    int size;
}stack;

/* Functions */
stack * create_stack()
{
    /* create/initialize empty stack */
    stack * s = malloc(sizeof(stack));
    s->start = NULL;
    s->end = NULL;
    s->size = 0;

    return s;
}

stack_node * create_node(int block_num)
{
    /* Create a node with the value we need */
    stack_node * node = malloc(sizeof(stack_node));
    node->block_num = block_num;
    node->next = NULL;

    return node;
}

void push(stack * s, int block_num)
{
    /* Create a node with the value we need */
    stack_node * node = create_node(block_num);
    if(s->size==0)
    {   /*first element in stack*/
        s->start = node;
        s->end = node;
    }
    else
    {   /*N-th element in stack*/
        s->end->next = node;
        s->end = node;
    }
    s->size ++;
}

int pop(stack *s)
{
    /*stack is empty or we are on the root*/
    if(s->size ==0)
        return -1;

    /*find the pre-last*/
    stack_node * temp = s->start;
    int i;
    for (i=1 ; i <s->size -1; i++)
        temp = temp->next;

    /*store the last one and pop from stack*/
    stack_node * poped = s->end;
    s->end = temp;

    /*get the block number value*/
    int num = poped->block_num;
    /*free the node that poped*/
    free(poped);
    s->size--;
    if(s->size ==0)
        s->start = NULL;

    /*return the value*/
    return num;
}

void clean_stack(stack * s)
{
    /*pop until the stack is empty*/
    while(pop(s)!=-1){}
    free(s);
}


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

int CompareKeys (void *ptr1 , void *ptr2 , char type)
{
	if (type == 'c')
	{
		char * a = (char *) ptr1;
		char *b = (char *) ptr2;
		return strcmp(a , b);
	}
	if (type == 'i')
	{
		int a = *(int *) ptr1;
		int b = *(int *) ptr2;
		return a-b;

	}
	else
		{
			float a = *(float *) ptr1;
			float b = *(float *) ptr2;
			if (a-b < 0.0)
				return -1;
			else if(a-b > 0.0)
					return 1;
			else
					return 0;
		}
}

/*Upon successful completion, the open function shall open the file and return
a non-negative integer representing the lowest numbered unused file descriptor.
By this way, */
int hashfile(int fd)
{
    return fd%MAXFILES;
}

void write_value(char attr, int length, char * data, void * value)
{
    if(attr == 'c')
        strcpy(data, value);
    else
        memcpy(data , value , length);
}

void read_value(char * data, void * value, int length)
{
	value = malloc(length);
	memcpy(value, data , length);
}

void read_int_value(char * data, int * num)
{
	memcpy(num, data, sizeof(int));
}


/****************************************************
            ~~Scan Assistant Functions~~
****************************************************/

scan_point * get_most_left(int fileDesc)
{
	int root = open_files[hashfile(fileDesc)].root;
	int kid, counter;
	// get_blocks_left(root)
	checkBF(BF_GetBlock(fileDesc, root, block));
	char * data = BF_Block_GetData(block);

	while(*data == 'E')
	{
		data += sizeof(char);
		read_int_value(data, &counter);
		data += sizeof(int);

		if (counter ==0)
		{
			printf("ERROR IN BLOCK\n" );
			exit(EXIT_FAILURE);
		}

		read_int_value(data, &kid);
		data += sizeof(int);
		if(kid == -1)
		{
			data += open_files[hashfile(fileDesc)].attrLength1;
			read_int_value(data, &kid);
		}
		checkBF(BF_UnpinBlock(block));
		checkBF(BF_GetBlock(fileDesc, kid, block));
		data = BF_Block_GetData(block);
	}

	if(*data != 'D')
	{
		printf("FUCKING ERROR GG BOYS\n" );
		exit(EXIT_FAILURE);
	}

	data += sizeof(char);
	int next;
	read_int_value(data, &next);
	data += sizeof(int);
	read_int_value(data, &counter);
	data += sizeof(int);

	scan_point * current;
	current = malloc(sizeof(scan_point));
	read_value(data, current->value1, open_files[hashfile(fileDesc)].attrLength1);
	data += open_files[hashfile(fileDesc)].attrLength1;
	read_value(data, current->value2, open_files[hashfile(fileDesc)].attrLength2);

	current->block_num = kid;
	current->record_num = 0;
	current->max_record = counter;
	current->next_block = next;

	checkBF(BF_UnpinBlock(block));


	return current;
}

void scan_get_next_value(scan_point * p, int fileDesc)
{
	if(p->record_num == p->max_record)
	{/* go the next block */
		if(p->next_block == -1)
		{
			free(p->value1);
			free(p->value2);
			p->value1 = NULL;
			p->value1 = NULL;
			return;
		}
		checkBF(BF_GetBlock(fileDesc, p->next_block, block));
		char * data = BF_Block_GetData(block);

		if(*data != 'D')
		{
			printf("FUCKING ERROR GG BOYS\n" );
			exit(EXIT_FAILURE);
		}

		data += sizeof(char);
		int next;
		read_int_value(data, &next);
		data += sizeof(int);
		int counter ;
		read_int_value(data, &counter);
		data += sizeof(int);

		free(p->value1);
		read_value(data, p->value1, open_files[hashfile(fileDesc)].attrLength1);
		data += open_files[hashfile(fileDesc)].attrLength1;
		free(p->value2);
		read_value(data, p->value2, open_files[hashfile(fileDesc)].attrLength2);


		p->block_num = p->next_block;
		p->record_num = 0;
		p->next_block = next;
		p->max_record = counter;

		checkBF(BF_UnpinBlock(block));
	}
	else
	{
		checkBF(BF_GetBlock(fileDesc, p->block_num, block));
		char * data = BF_Block_GetData(block);

		int size_of_record = open_files[hashfile(fileDesc)].attrLength1 + open_files[hashfile(fileDesc)].attrLength2;

		data += sizeof(char);
		data += (2*sizeof(int));
		data += (p->record_num*size_of_record);

		p->record_num ++;
		free(p->value1);
		read_value(data, p->value1, open_files[hashfile(fileDesc)].attrLength1);
		data += open_files[hashfile(fileDesc)].attrLength1;
		free(p->value2);
		read_value(data, p->value2, open_files[hashfile(fileDesc)].attrLength2);

		checkBF(BF_UnpinBlock(block));
	}
}

int scan_check(void * main_value, int op, void * current_value, char type)
{
	if(current_value == NULL)
		return -1;

	int result = CompareKeys(main_value, current_value, type);

	switch (op) {
		case EQUAL:
			if (result == 0)
				return 1;
			return -1;
		case NOT_EQUAL:
			if(result == 0)
				return 0;
			return 1;
		case LESS_THAN:
			if(result >0)
				return 1;
			return -1;
		case GREATER_THAN:
			if(result < 0)
				return 1;
			else if( result == 0)
				return 0;
		case LESS_THAN_OR_EQUAL:
			if(result >= 0)
				return 1;
			return -1;
		case GREATER_THAN_OR_EQUAL:
			if(result <= 0)
				return 1;
			return 0;
		default:
			printf("ERROR IN OPERATOR\n" );
			return -1;
	}


}
/****************************************************
                ~~AM Functions~~
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
        printf("File %s deleted\n", fileName);
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

  printf("FILE DESC IN OPEN: %d\n",fd);

	char *data = BF_Block_GetData(block);

	if(memcmp(data,"AM_Index",sizeof("AM_Index")) != 0){
		printf("Error! Wrong type of file\n");
		return -1;
	}

  int index = hashfile(fd);
	strcpy(open_files[index].filename,fileName);
	open_files[index].fileDesc = fd;

	data += sizeof("AM_Index");
	memcpy(&open_files[index].attr1,data,sizeof(char));
	data += sizeof(char);
	memcpy(&open_files[index].attrLength1,data,sizeof(int));
	data += sizeof(int);
	memcpy(&open_files[index].attr2,data,sizeof(char));
	data += sizeof(char);
	memcpy(&open_files[index].attrLength2,data,sizeof(int));

	// Tests to check the function
	printf("FileName: %s\n",open_files[index].filename);
	printf("FileDesc: %d\n",open_files[index].fileDesc);
	printf("Attr1: %c\n",open_files[index].attr1);
	printf("AttrLength1: %d\n",open_files[index].attrLength1);
	printf("Attr2: %c\n",open_files[index].attr2);
	printf("attrLength2: %d\n\n",open_files[index].attrLength2);

  BF_UnpinBlock(block);

  return fd;
}


int AM_CloseIndex (int fileDesc) {

  	// Locate the position of the file
	int index = hashfile(fileDesc);

	if(open_files[index].fileDesc == -1){
		printf("Error! File is not Open\n");
		return AME_CANTCLOSE;
	}

	// Check whether there are any open scans
	for(int i = 0; i < MAXSCANS; i++){
		if(scans[i].fileDesc == index){
			printf("Error! There are open scans for the file\n");
			return AME_CANTCLOSE;
		}
	}

	/* Everything's fine,
		remove the file */
	checkBF(BF_CloseFile(open_files[index].fileDesc));
  open_files[index].fileDesc = -1;

  	return AME_OK;
}

int AM_InsertEntry(int fileDesc, void *value1, void *value2) {

	int index = hashfile(fileDesc);
	int blocks_num;
	char *data;
	int counter;
	int null_pointer = -1;
	checkBF(BF_GetBlockCounter(fileDesc , &blocks_num));

	if (blocks_num == 1)
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
        write_value(open_files[index].attr1, open_files[index].attrLength1, data, value1);
		data += open_files[index].attrLength1;
		int tmp = blocks_num + 1;
        memcpy(data , &tmp , sizeof(int));
        BF_Block_SetDirty(block);
        checkBF(BF_UnpinBlock(block));


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
		// memcpy(data , value1 , open_files[index].attrLength1);
        write_value(open_files[index].attr1, open_files[index].attrLength1, data, value1);
		data += open_files[index].attrLength1;
		// // memcpy(data , value2 , open_files[index].attrLength2);
        write_value(open_files[index].attr2, open_files[index].attrLength2, data, value2);

        BF_Block_SetDirty(block);
        checkBF(BF_UnpinBlock(block));
	}


  return AME_OK;
}


int AM_OpenIndexScan(int fileDesc, int op, void *value) {
	int i;

	if(openscans == MAXSCANS)
		return AME_MAXSCANS;

	int scanDesc = -1;
	for(i = 0 ; i < MAXSCANS; i++)
	{
		if(scans[i].fileDesc ==-1)
			scanDesc = i;
	}

	scans[scanDesc].fileDesc = fileDesc;
	scans[scanDesc].op = op;
	scans[scanDesc].value = value;
	if((op == NOT_EQUAL) || (op == LESS_THAN) || (op == GREATER_THAN))
		scans[scanDesc].current = get_most_left(fileDesc);

	openscans++;
  	return scanDesc;
}



void *AM_FindNextEntry(int scanDesc){
	Scan * my_scan = &scans[scanDesc];
	void * return_value;


	int result = scan_check(my_scan->value, my_scan->op, my_scan->current->value1, open_files[my_scan->fileDesc].attr1);
	if(result == 0)
   	{
   		while(result ==0)
		{
  	   		scan_get_next_value(my_scan->current, my_scan->fileDesc);
			result = scan_check(my_scan->value, my_scan->op, my_scan->current->value1, open_files[my_scan->fileDesc].attr1);
		}
   	}


	if(result == -1)
	{
		my_scan->current->value1 = NULL;
		return_value = NULL;
	}
	else
	{
		return_value = my_scan->current->value2;
		scan_get_next_value(my_scan->current, my_scan->fileDesc);
	}

	return return_value;
}


int AM_CloseIndexScan(int scanDesc) {
	if(scans[scanDesc].fileDesc == -1)
		return AME_SCANCLOSED;
	openscans--;
	scans[scanDesc].fileDesc = -1;
	free(scans[scanDesc].current->value1);
	free(scans[scanDesc].current->value2);
	free(scans[scanDesc].current);

	return AME_OK;
}


void AM_PrintError(char *errString) {

}

void AM_Close() {
    BF_Block_Destroy(&block);
    BF_Close();
}
