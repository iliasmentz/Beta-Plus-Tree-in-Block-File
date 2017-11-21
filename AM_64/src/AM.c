#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
	int root;
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
    int first;
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
BF_Block * temp_block;
BF_Block * new_block;
BF_Block * new_root;

/****************************************************
				~~Assistive Functions~~
****************************************************/

void write_value(char attr, int length, char * data, void * value)
{
		if(attr == 'c')
				strcpy(data, value);
		else
				memcpy(data , value , length);
}

void read_value(char * data, void ** value, int length)
{
	*value = malloc(length);
	memcpy(*value, data , length);
}


void read_int_value(char * data, int * num)
{
	memcpy(num, data, sizeof(int));
}

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


/* Used while trying to traverse the index blocks
	of the tree.
	Helps to find the correct data block, if such a block
	exists. Otherwise, it returns -1 */
int getChildBlock(int counter, char *data, int attrLength1,
	char attr1, void *value1){

	int current = 0;
	int block_num;

    data += sizeof(int);
    for(int i = 0; i < counter; i++)
    {
        current = CompareKeys(data, value1, attr1);

        if(current > 0)
        {
            memcpy(&block_num, data-sizeof(int), sizeof(int));
            return block_num;
        }
        data += (sizeof(int)+attrLength1);
    }
    memcpy(&block_num, data-sizeof(int), sizeof(int));
    return block_num;
}

int writeNums(int counter, char *data, int attrLength1, char attr1, void *value1, int block_num)
{

	int current = 0;
    int next;
    data += sizeof(int);
    for(int i = 0; i < counter; i++)
    {
        current = CompareKeys(data, value1, attr1);

        if(current > 0)
        {
            memcpy(data-sizeof(int), &block_num, sizeof(int));
            read_int_value(data+attrLength1, &next);
            return next;
        }
        data += (sizeof(int)+attrLength1);
    }
    memcpy(data-sizeof(int), &block_num, sizeof(int));

    read_int_value(data-attrLength1-sizeof(int), &next);
    return next;

}

/* Inserts a new entry at a data block which still
	 has some space left
*/
void insert_AvailableSpace(int counter, char *data,
	int record_size,void *value1, void *value2, char attr1, char attr2,
	int attr1_size, int attr2_size){

    /* Find the correct location where we'll insert
		 the new entry
	*/
	for(int i = 0; i < counter; i++)
    {

		/* Move all entries and insert the new one
			at the front
		*/
		if(CompareKeys(data, value1,attr1) > 0)
        {
			memmove(data+record_size, data,(counter-i)*record_size);

			write_value(attr1, attr1_size, data, value1);
			write_value(attr2, attr2_size, data+attr1_size, value2);

			return;
		}
        data += record_size;
	}
    write_value(attr1, attr1_size, data, value1);
    write_value(attr2, attr2_size, data+attr1_size, value2);
}


/****************************************************
            ~~Scan Assistant Functions~~
****************************************************/

scan_point * get_most_left(int fileDesc)
{
	int root = open_files[hashfile(fileDesc)].root;
	int kid, counter;

    /* go to the root */
	checkBF(BF_GetBlock(fileDesc, root, block));
	char * data = BF_Block_GetData(block);

	while(*data == 'E')
	{   /* while you are on index block, find its most left pointer */
		data += sizeof("E");
		read_int_value(data, &counter);

        // printf("Counter %d\n", counter );
		data += sizeof(int);

		if (counter ==0)
		{
			printf("ERROR IN BLOCK\n" );
			exit(EXIT_FAILURE);
		}

		read_int_value(data, &kid);
		data += sizeof(int);
		if(kid == -1)
		{   /*his first data has no left kid */
			data += open_files[hashfile(fileDesc)].attrLength1;
			read_int_value(data, &kid);
		}

		checkBF(BF_UnpinBlock(block));
		checkBF(BF_GetBlock(fileDesc, kid, block));
		data = BF_Block_GetData(block);
	}

	if(*data != 'D')
	{
		printf("Error in B+Tree Structure \n" );
		exit(EXIT_FAILURE);
	}

    /* go to the first value */
	data += sizeof("D");
	int next;
	read_int_value(data, &next);
	data += sizeof(int);
	read_int_value(data, &counter);
	data += sizeof(int);

    /* create the scan point */
	scan_point * current;
	current = malloc(sizeof(scan_point));
    /* get the 1st value */
	read_value(data, &current->value1, open_files[hashfile(fileDesc)].attrLength1);
    if(current->value1 == NULL)
    {
        printf("Error in B+Tree Structure \n" );
		exit(EXIT_FAILURE);
    }
    /* get the 2nd value */
	data += open_files[hashfile(fileDesc)].attrLength1;
	read_value(data, &current->value2, open_files[hashfile(fileDesc)].attrLength2);

    /* write the values we are gonna need */
	current->block_num = kid;
	current->record_num = 0;
	current->max_record = counter;
	current->next_block = next;


	checkBF(BF_UnpinBlock(block));


	return current;
}


scan_point * get_value_point(int fileDesc, void * value)
{
    int root = open_files[hashfile(fileDesc)].root;
    checkBF(BF_GetBlock(fileDesc, root, block));
    char * data = BF_Block_GetData(block);
    int counter;
    int kid;
    while(*data == 'E')
    {   /* while you are on index block, find  the suitable kid */
        data += sizeof("E");
        read_int_value(data, &counter);
        data += sizeof(int);

        /*move to first kid */
        kid = getChildBlock(counter, data, open_files[hashfile(fileDesc)].attrLength1, open_files[hashfile(fileDesc)].attr1, value);
        checkBF(BF_UnpinBlock(block));
        checkBF(BF_GetBlock(fileDesc, kid, block));
        data = BF_Block_GetData(block);
    }

    if(*data != 'D')
	{
		printf("Error in B+Tree Structure \n" );
		exit(EXIT_FAILURE);
	}

    /* go to the first value */
	data += sizeof("D");
	int next;
	read_int_value(data, &next);
	data += sizeof(int);
	read_int_value(data, &counter);
	data += sizeof(int);

    /* create the scan point */
	scan_point * current;
	current = malloc(sizeof(scan_point));
    /* get the 1st value */
	read_value(data, &current->value1, open_files[hashfile(fileDesc)].attrLength1);
    if(current->value1 == NULL)
    {
        printf("Error in B+Tree Structure \n" );
		exit(EXIT_FAILURE);
    }
    /* get the 2nd value */
	data += open_files[hashfile(fileDesc)].attrLength1;
	read_value(data, &current->value2, open_files[hashfile(fileDesc)].attrLength2);

    /* write the values we are gonna need */
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
		{   /* there is no next block */
			free(p->value1);
			free(p->value2);
			p->value1 = NULL;
			p->value2 = NULL;
			return;
		}
		checkBF(BF_GetBlock(fileDesc, p->next_block, block));
		char * data = BF_Block_GetData(block);

		if(*data != 'D')
		{
			printf("Error in B+ Tree Structure\n" );
			exit(EXIT_FAILURE);
		}

        /* get the counter and next and move to data */
		data += sizeof("D");
		int next;
		read_int_value(data, &next);
		data += sizeof(int);
		int counter ;
		read_int_value(data, &counter);
		data += sizeof(int);

        /* get the next values */
		free(p->value1);
		read_value(data, &p->value1, open_files[hashfile(fileDesc)].attrLength1);
		data += open_files[hashfile(fileDesc)].attrLength1;
		free(p->value2);
		read_value(data, &p->value2, open_files[hashfile(fileDesc)].attrLength2);

        /* get the data you need to be ready for the next scan */
		p->block_num = p->next_block;
		p->record_num = 1;
		p->next_block = next;
		p->max_record = counter;

		checkBF(BF_UnpinBlock(block));
	}
	else
	{   /*take the next data from current block */
		checkBF(BF_GetBlock(fileDesc, p->block_num, block));
		char * data = BF_Block_GetData(block);

		int size_of_record = open_files[hashfile(fileDesc)].attrLength1 + open_files[hashfile(fileDesc)].attrLength2;

		data += sizeof("D");
		data += (2*sizeof(int));
		data += (p->record_num*size_of_record);

        /* get the next values */
		free(p->value1);
		read_value(data, &p->value1, open_files[hashfile(fileDesc)].attrLength1);
		data += open_files[hashfile(fileDesc)].attrLength1;
		free(p->value2);
		read_value(data, &p->value2, open_files[hashfile(fileDesc)].attrLength2);

        /* move the record counter to the next record */
        p->record_num ++;

		checkBF(BF_UnpinBlock(block));
	}
    // printf("Getting next values: %f %s \n", *(float * )p->value1,(char *)p->value2 );
}

int scan_check(void * main_value, int op, void * current_value, char type)
{
    /* if our current value is NULL, we reached the end of the search */
	if(current_value == NULL)
    {
        return -1;
    }

    /* compare our main value with the current value */
	int result = CompareKeys(main_value, current_value, type);

	switch (op) {
		case EQUAL:
			if (result == 0)
				return 1;
			return 0;
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
			// else if( result == 0)
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
				~~AM FUNCTIONS~~
****************************************************/

int AM_errno = AME_OK;

void AM_Init() {
		BF_Init(LRU);
		BF_Block_Init(&block);
		BF_Block_Init(&temp_block);
		BF_Block_Init(&new_block);
		BF_Block_Init(&new_root);

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
    if ( (attrType1 == 'i' && attrLength1!=sizeof(int)) || (attrType2 == 'i' && attrLength2!=sizeof(int)) ||
    (attrType1 == 'f' && attrLength1!=sizeof(float)) || (attrType2 == 'f' && attrLength2!=sizeof(float)) )
    {
    	 AM_errno = AME_WRONG_TYPES;
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

	memcpy(data, &attrLength1, sizeof(int));
	data += sizeof(int);

	(*data) = attrType2;
	data += sizeof(char);


	memcpy(data, &attrLength2, sizeof(int));
    data += sizeof(int);

	int root=1;
	memcpy(data, &root, sizeof(int));
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
								return AM_errno;
						}
				}
		}

		int status = remove(fileName);
		if(status ==0)
		{
				printf("File %s deleted\n", fileName);
				return AME_OK;
		}
		else
		{
						AM_errno = AME_CANTDESTROY;
						return AME_OK;
		}
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
    data += sizeof(int);
    read_int_value(data, &open_files[index].root);

	// Tests to check the function
	// printf("FileName: %s\n",open_files[index].filename);
	// printf("FileDesc: %d\n",open_files[index].fileDesc);
	// printf("Attr1: %c\n",open_files[index].attr1);
	// printf("AttrLength1: %d\n",open_files[index].attrLength1);
	// printf("Attr2: %c\n",open_files[index].attr2);
	// printf("attrLength2: %d\n\n",open_files[index].attrLength2);

	BF_UnpinBlock(block);

	return fd;
}


int AM_CloseIndex (int fileDesc) {

	int index = hashfile(fileDesc);

	if(open_files[index].fileDesc == -1){
		printf("Error! File is not Open\n");
		return AME_FILENOTOPEN;
	}

	// Check whether there are any open scans
	for(int i = 0; i < MAXSCANS; i++){
		if(scans[i].fileDesc == index){
			printf("Error! There are open scans for the file\n");
			return AME_SCANOPEN;
		}
	}

	/* Everything's fine,
		remove the file */
	checkBF(BF_CloseFile(open_files[index].fileDesc));
	open_files[index].fileDesc = -1;

		return AME_OK;
}

void Ascend(stack *s ,int fileDesc,int index,int max_keys,int attr1_size, void *to_go_up , void *temp_up )
{
    // printf("SPAW\n" );
    int thesi;
    int x = pop(s);
    int blocks_num;
    checkBF(BF_GetBlockCounter(fileDesc ,&blocks_num));
    blocks_num--;

    while(1)
    {
        int counter;
    	char* data;
        //kathe fora 8a pernw oti kanei pop h stoiva
        checkBF(BF_GetBlock(fileDesc , x , block));
        data = BF_Block_GetData(block);
        data += sizeof("E");
        memcpy(&counter , data , sizeof(int));
        data += 2*sizeof(int);
        //kanw to Get_BlockCounter gia na krataw poio einai to neo block
        //checkBF(BF_GetBlockCounter( fileDesc , &blocks_num));
        //blocks_num--;

        if (counter < max_keys)
        {
            thesi = 0;
            while(1)
            {

                if (CompareKeys(to_go_up , data , open_files[index].attr1) < 0)
                    break;
                else
                {
                //metakinw kata ena kleidi kai deikth dhladh ftanw sto epomeno kleidi
                //mia if akoma an mhn to afhsw na deiksei se thesh pou den exei timh
                    if (thesi == counter)
                        break;
                    data += attr1_size + sizeof(int);
                    thesi++;
                }
            }

            //se auth th fash o data deixnei sto kleidi pou prepei na metakinh8ei
            if (thesi != counter)
                memmove(data + attr1_size + sizeof(int) , data , (counter-thesi)*(attr1_size+sizeof(int)));
            //write_value
            write_value(open_files[index].attr1, attr1_size, data, to_go_up);
            // memcpy(data , to_go_up , attr1_size);
            //edw vazoume ton ari8mo tou block pou ftiaksame alla den evriska pote to perneis sthn ulopoihsh sou
            //ekana sthn arxh get BF_GetBlockCounter
            data += attr1_size;
            memcpy(data ,&blocks_num , sizeof(int));
            //====update counter ========////
            counter++;
            data -= (thesi*(attr1_size + sizeof(int)) + attr1_size +2*sizeof(int));
            memcpy(data , &counter , sizeof(int));
            free(to_go_up);
            BF_Block_SetDirty(temp_block);
            BF_Block_SetDirty(block);
            checkBF(BF_UnpinBlock(temp_block));
            checkBF(BF_UnpinBlock(block));
            break;
        }
        else
        {
            //efoson den exei xwro ftiaxnoume neo block
            //BF_Block *new_block;
            checkBF(BF_AllocateBlock(fileDesc , new_block));

            char *new_data;
            new_data = BF_Block_GetData(new_block);

            int newcounter = max_keys/2 -1;
            int newcounter1 = max_keys/2;
            //o deikths data deixnei sthn prwth 8esh
            //auth h if ginetai gia na pame ston meso pou anebasoume sthn nea riza
            if (max_keys%2 == 0)
            {
                counter = max_keys/2 -1;
                memcpy(data - 2*sizeof(int),&newcounter , sizeof(int));
                data += (max_keys/2 -1)*(attr1_size + sizeof(int));
            }
            else
            {
                counter = max_keys/2;
                memcpy(data - 2*sizeof(int),&newcounter1 , sizeof(int));
                data += (max_keys/2)*(attr1_size + sizeof(int));
            }
            read_value(data , &temp_up ,attr1_size);
            //char *tmdata = data;
            int currentblock;
            checkBF(BF_GetBlockCounter(fileDesc , &currentblock));
            currentblock--;
            memcpy(new_data , "E" , sizeof("E"));
            new_data  += sizeof("E");
            memcpy(new_data , &newcounter1 , sizeof(int));
            new_data += sizeof(int);
            data += attr1_size;
            memmove(new_data , data , (newcounter1*(attr1_size + sizeof(int)) + sizeof(int)));
            //tha eisagoume to neo kleidi
            //kai twra 8a eisagoume to neo kleidi
            //upen8umizw oti o temp_data deixnei sto kleidi pou 8eloume na metaferoume
            //kai oti o blocks_num deixnei se poio block vrisketai
            if(CompareKeys(to_go_up , temp_up ,open_files[index].attr1) < 0)
            {
                //to kleidi 8a mpei sto aristero paidi
                //to data ksanadeixnei sto teleytaio ths stoixeio
                data -= (sizeof(int) + 2*attr1_size);
                //kai twra ton ksanametakinw sto prwto stoixeio
                data -=((sizeof(int)+attr1_size)*(counter-1));
                counter ++;
                memcpy(data-(2*sizeof(int)), &counter, sizeof(int));
                thesi = 0;
                while(1)
                {
                    //otan mpei edw frontise na mhn ginei ovewright to data
                    if (CompareKeys(to_go_up , data , open_files[index].attr1) < 0)
                        break;
                    else
                    {
                        //na tsekarw ama ksefeugei
                        //metakinw kata ena kleidi kai deikth dhladh ftanw sto epomeno kleidi
                        if (thesi == counter-1)
                            break;
                        data += attr1_size + sizeof(int);
                        thesi++;
                    }
                }

                //se auth th fash o data deixnei sto kleidi pou prepei na metakinh8ei
                if (thesi != counter-1)
                    memmove(data + attr1_size + sizeof(int) , data , (max_keys-thesi)*(attr1_size+sizeof(int)));

                write_value(open_files[hashfile(fileDesc)].attr1, attr1_size, data, to_go_up);
                // memcpy(data , to_go_up , attr1_size);
                data += attr1_size;
                memcpy(data ,&blocks_num , sizeof(int) );

                //BF_Block_SetDirty(block);
                BF_Block_SetDirty(block);
                //BF_Block_SetDirty(temp_block);
                BF_Block_SetDirty(new_block);
                checkBF(BF_UnpinBlock(block));
                //checkBF(BF_UnpinBlock(temp_block));
                checkBF(BF_UnpinBlock(new_block));
                //break;
            }
            else
            {
                //to kleidi 8a mpei sto deksi block
                //ypen8umizw oti o new_data deikths deixnei ekei pou brisketai o prwtos deikths tou eurethriou
                //ara metakinw kata sizeof(int) gia na paw sthn prwth eggrafh
				newcounter1++;
				memcpy(new_data - sizeof(int),&newcounter1,sizeof(int) );
                new_data += sizeof(int);
                thesi = 0;
                while(1)
                {
                    if (CompareKeys(to_go_up , new_data , open_files[index].attr1) < 0)
                        break;
                    else
                    {
                        //metakinw kata ena kleidi kai deikth dhladh ftanw sto epomeno kleidi
                        if (thesi == newcounter1-1)
                            break;
                        new_data += attr1_size + sizeof(int);
                        thesi++;
                    }
                }

                //se auth th fash o data deixnei sto kleidi pou prepei na metakinh8ei
                if (thesi == newcounter1-1)
                    memmove(new_data + attr1_size + sizeof(int) , new_data , (max_keys-thesi)*(attr1_size+sizeof(int)));
                write_value(open_files[hashfile(fileDesc)].attr1, attr1_size, new_data, to_go_up);
                // memcpy(new_data , to_go_up , attr1_size);
                data += attr1_size;
                memcpy(new_data ,&blocks_num , sizeof(int) );

                BF_Block_SetDirty(block);
                BF_Block_SetDirty(new_block);
                checkBF(BF_UnpinBlock(block));
                checkBF(BF_UnpinBlock(new_block));
                //break;
            }
            if(s->size = 0 )
            {
                printf("NEA RIZa\n" );
                BF_Block *new_root;
                checkBF(BF_AllocateBlock(fileDesc ,new_root));
                char *root_data = BF_Block_GetData(new_root);
                memcpy(root_data , "E" ,sizeof("E"));
                root_data += sizeof("E");
                int h =1;
                memcpy(root_data , &h , sizeof(int));
                root_data += sizeof(int);
                memcpy(root_data , &x , sizeof(int));
                root_data += sizeof(int);
                write_value(open_files[index].attr1 , attr1_size , root_data , temp_up);
                root_data += attr1_size;
                memcpy(root_data , &currentblock , sizeof(int));
								checkBF(BF_GetBlock(fileDesc ,0 ,temp_block ));
                int newroot;
                checkBF(BF_GetBlockCounter(fileDesc ,&newroot));
                newroot--;
								char* pdata = BF_Block_GetData(temp_block);
								pdata += sizeof("AM_Index") + 2*sizeof(int) +2*sizeof(char);
								memcpy(data , &newroot , sizeof(int));
                open_files[index].root = newroot;
								//checkBF(BF_GetBlock(fileDesc , ))
                BF_Block_SetDirty(new_root);
                checkBF(BF_UnpinBlock(new_root));
                BF_Block_SetDirty(block);
                BF_UnpinBlock(block);
                break;
            }
            //an ftasoume edw shmainei oti den spasame riza ara prepei na anevoume ki allo
            //ara ananewnw to kleidi pou prepei na anevasw to opoio einai to tmdata
            //kai to blocks_num pou to kanw iso me to current block
            x = pop(s);
            blocks_num = currentblock;
            //fix einai metablhth etsi wste otan kleinei to
    		free(to_go_up);
            read_value(temp_up , &to_go_up, attr1_size);
            free(temp_up);
        }
      }
      // if(to_go_up !=NULL)
      //   free(to_go_up);
      // if(temp_up !=NULL)
      //   free(temp_up);
}

int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
    for(int i =0 ; i <MAXSCANS ; i++)
    {
        if(scans[i].fileDesc == fileDesc)
        {
            return AME_SCANOPEN;
        }
    }

    // printf("Adding %d\n",*(int *)value1 );
	void * to_go_up;
	void * temp_up;

	int index = hashfile(fileDesc);

	int blocks_num;
	char *data;
	char * temp_data;

	int counter;
	int traversal;

	int attr1_size = open_files[index].attrLength1;
	int attr2_size = open_files[index].attrLength2;
	int record_size = attr1_size + attr2_size;

	int a = (BF_BLOCK_SIZE-2*sizeof(int)-sizeof("D"));
	int b = (attr1_size+ attr2_size);
	int max_data = a/b;

	int c = (BF_BLOCK_SIZE-sizeof("E")-2*sizeof(int));
	int d = (attr1_size +sizeof(int));
	int max_keys = c/d;

	int null_pointer = -1;
	int child_block;

	char attr1 = open_files[index].attr1;
	char attr2 = open_files[index].attr2;

    stack *s = create_stack();
    push(s, open_files[index].root);

	checkBF(BF_GetBlockCounter(fileDesc , &blocks_num));

	/* The file contains only one block,
		which holds information about it.
		Therefore, we start creating the tree */
	if (blocks_num == 1)
	{
		// Creating the first index block
		// Each index block starts with an "E"
		checkBF(BF_AllocateBlock(fileDesc , block));
		data = BF_Block_GetData(block);
		memcpy(data , "E" ,sizeof("E"));
		data += sizeof("E");
		counter = 1;
		memcpy (data , &counter , sizeof(int));
		data += sizeof(int);
		memcpy(data ,&null_pointer , sizeof(int));
		data += sizeof(int);
		write_value(attr1, attr1_size, data, value1);
		data += attr1_size;
		int tmp = blocks_num + 1;
		memcpy(data , &tmp , sizeof(int));
		BF_Block_SetDirty(block);
		checkBF(BF_UnpinBlock(block));


		// Creating the first data block
		// Each data block starts with a "D"
		checkBF(BF_AllocateBlock(fileDesc , block));
		data = BF_Block_GetData(block);
		memcpy(data , "D" , sizeof("D"));
		data += sizeof("D");
		memcpy(data , &null_pointer , sizeof(int));
		data += sizeof(int);
		memcpy(data , &counter , sizeof(int));
		data += sizeof(int);
		write_value(attr1, attr1_size, data, value1);
		data += attr1_size;
		write_value(attr2, attr2_size, data, value2);
		BF_Block_SetDirty(block);
		checkBF(BF_UnpinBlock(block));
	}

	/* Traverse the tree, in order to find the correct data
		block to place the new entry */
	else
	{
		checkBF(BF_GetBlock(fileDesc, open_files[index].root, block));
		data = BF_Block_GetData(block);
		data += sizeof("E");
		memcpy(&counter , data , sizeof(int));
		data += sizeof(int);



	 	while(1)
	 	{

			// Find the correct child node of the current block
			child_block = getChildBlock(counter,data,attr1_size,attr1,value1);
            // printf("child_block %d\n", child_block );
			/* The data block which satisfies the conditions
				hasn't been created yet */
			if(child_block == -1){
				// Find the block_num of the adjacent data block
				int next = writeNums(counter,data, attr1_size,
					attr1,value1,blocks_num);
				BF_Block_SetDirty(block);
				checkBF(BF_UnpinBlock(block));

				// Create the new data block
				checkBF(BF_AllocateBlock(fileDesc,block));
				checkBF(BF_GetBlock(fileDesc,blocks_num,block));
				data = BF_Block_GetData(block);

				memcpy(data,"D",sizeof("D"));
				data += sizeof("D");
				memcpy(data,&next,sizeof(int));
				data += sizeof(int);
				int tmp1 = 1;
				memcpy(data,&tmp1,sizeof(int));
				data += sizeof(int);
				write_value(attr1, attr1_size, data, value1);
				data += attr1_size;
				write_value(attr2, attr2_size, data, value2);

				BF_Block_SetDirty(block);
				checkBF(BF_UnpinBlock(block));

				break;
			}

			// Get the block we just found
            checkBF(BF_UnpinBlock(block));
			checkBF(BF_GetBlock(fileDesc,child_block,block));
			data = BF_Block_GetData(block);

			// Check whether the child block is a data block
			if(memcmp(data,"D",sizeof("D")) == 0){

				data += sizeof("D") + sizeof(int);
				memcpy(&counter, data, sizeof(int));
				data += sizeof(int);


				/* If the data block has some space available,
					 insert the new entry
				*/
				if(counter < max_data)
                {
					insert_AvailableSpace(counter, data, record_size,	value1, value2, attr1, attr2, attr1_size, attr2_size);
                    counter ++;
                    memcpy(data-sizeof(int), &counter, sizeof(int));
                    BF_Block_SetDirty(block);
                    checkBF(BF_UnpinBlock(block));
                }
				/* There's not enough space available, so we
					 must split the current block into two
				*/

				else
				{
					checkBF(BF_AllocateBlock(fileDesc ,temp_block));
					temp_data = BF_Block_GetData(temp_block);
					memcpy(temp_data, "D", sizeof("D"));
					temp_data += sizeof("D");

					// **This only applies when the original block doesn't have any neighbors**
					// **The new block acquires the neighbor of the original block as its own**
					memcpy(temp_data , data-(2*sizeof(int)) , sizeof(int));
                    checkBF(BF_GetBlockCounter(fileDesc , &blocks_num));
                    blocks_num--;
                    memcpy(data-(2*sizeof(int)), &blocks_num, sizeof(int));

					// Move the pointer to the location of the first entry
					temp_data += 2*sizeof(int);

					/* metakinw ton deikth sthn 8esh ths eggrafhs h opoia
					8a metafer8ei sto kainourio block
					oi prwtes max_data/2 eggrafes 8a meinoun kai oi
					upoloipes 8a pane sto kainourio
					*/
					// Move to the location where the split happens
					// data += (max_data/2)*(record_size);
					data += ((int)ceil((double)max_data/2.0))*(record_size);

					/*These two counters are used to keep track of the number
					of entries that will go to each block */

					// int tmp_counter = max_data/2;
					int tmp_counter = (int)ceil((double)max_data/(2.0));
					int tmp_counter1 = max_data - (int)ceil((double)max_data/2.0);


					/********************************************************
					Entries which share the same key must not be seperated
					********************************************************/
					/* Check to see whether the key is the same as both the
					 previous one and the next one
					*/

					// What if the pointer is pointing at the last entry??
					// NEEDS A COUNTER (maybe)

                    if(CompareKeys(data, data-record_size, attr1)==0)
                    {
                        int left_same = 1;
                        int tmp2 = tmp_counter;

                        while((CompareKeys(data, data-(left_same+1)*record_size, attr1) ==0) && (tmp2 >0))
                        {
                            left_same++;
                            tmp2--;
                        }

                        if(tmp2 == 0)
                        {
                            int right_same = 0;
                            int tmp2 = tmp_counter;

                            while((CompareKeys(data, data-(right_same+1)*record_size, attr1) ==0) && (tmp2 < max_data))
                            {
                                right_same++;
                                tmp2--;
                            }
                            if(tmp2 == max_data)
                            {
                                printf("ERROR BUCKET FULL WITH SAME VALUES\n" );
                                return AME_FULLBUCKET;
                            }
                            tmp_counter +=right_same;
                            data += right_same*record_size;
                            tmp_counter1 = max_data - tmp_counter;
                        }
                        else
                        {
                            tmp_counter -=left_same;
                            data -= left_same*record_size;
                            tmp_counter1 = max_data - tmp_counter;
                        }
                    }

					/* sth periptwsh pou to kleidi einai to prwto idio den
					 xreiazete na kanoume kati epeidh 8a metakinh8ou
					 n ola sto epomeno block automata
					*/

					/* Distribute half of the original block's entries to
					 the recently allocated block
					*/
					memmove(temp_data, data, tmp_counter1*record_size);
					/*for (int j = tmp_counter; j < max_data;  j++)
					{
    					write_value(attr1 ,attr1_size ,temp_data , data);
    					data += attr1_size;
    					temp_data += attr1_size;
    					write_value(attr2 , attr1_size , temp_data , data);
    					data += attr2_size;
    					temp_data += attr2_size;
					}*/

					// Move both pointers to the first entry of each block
					data -= (tmp_counter)*record_size;
					//temp_data -= (tmp_counter1)*record_size;
                    memcpy(data-sizeof(int), &tmp_counter, sizeof(int));
                    memcpy(temp_data-sizeof(int), &tmp_counter1, sizeof(int));

					/* Check if the new entry will go to the original block.
					 If value1 is greater than the value pointed at
					 by temp_data,then we must push it to
					 the original block.
					*/

					/* The original block has less than half the entries
					 it's supposed to have.
					 Insert the new entry there
					*/
					if(CompareKeys(temp_data , value1 , attr1) > 0)
					{
                        insert_AvailableSpace(tmp_counter, data, record_size, value1, value2,  attr1,  attr2, attr1_size, attr2_size);
                        tmp_counter++;
                        memcpy(data-sizeof(int), &tmp_counter, sizeof(int));
					}
					//edw einai h periptwsh pou h eggrafh 8a mpei sto prwto block alla oxi sto telos
					//ara 8a broume se poia 8esh prepei na mpei gia na parameinei to block taksinomhmeno
					//h eggrafh 8a mpei sto deutero block
					else
					{
                        insert_AvailableSpace(tmp_counter1, temp_data, record_size, value1, value2,  attr1,  attr2, attr1_size, attr2_size);
                        tmp_counter1++;
                        memcpy(data-sizeof(int), &tmp_counter1, sizeof(int));
					}


                    read_value(temp_data, &to_go_up, attr1_size);
                    BF_Block_SetDirty(block);
                    checkBF(BF_UnpinBlock(block));
                    BF_Block_SetDirty(temp_block);
                    checkBF(BF_UnpinBlock(temp_block));
                    //main loop ths anavashs
                    Ascend(s ,fileDesc,index , max_keys,attr1_size, to_go_up , temp_up );

                }
				break;
			}

			// Repeat the process for the block we just found
			data += sizeof("E");
			memcpy(&counter,data,sizeof(int));
			data += sizeof(int);
			push(s, child_block);
		}
	}
    clean_stack(s);


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
    scans[scanDesc].first = 1;
	if((op == NOT_EQUAL) || (op == LESS_THAN) || (op == LESS_THAN_OR_EQUAL))
        scans[scanDesc].current = get_most_left(fileDesc);
    else
        scans[scanDesc].current = get_value_point(fileDesc, value);
	openscans++;
  	return scanDesc;
}


void *AM_FindNextEntry(int scanDesc) {
    Scan * my_scan = &scans[scanDesc];
	void * return_value;

    if(my_scan->first == 0)
    {
        scan_get_next_value(my_scan->current, my_scan->fileDesc);
    }
    else
    {
        my_scan->first = 0;
    }

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
        AM_errno = AME_EOF;
	}
	else
	{
        // read_value(my_scan->current->value2, &return_value, open_files[hashfile(my_scan->fileDesc)].attrLength2);
        return_value = my_scan->current->value2;
	}

	return return_value;
}


int AM_CloseIndexScan(int scanDesc) {
    if(scans[scanDesc].fileDesc == -1)
		return AME_SCANCLOSED;
	openscans--;
	scans[scanDesc].fileDesc = -1;
    if(scans[scanDesc].current->value1 != NULL)
	   free(scans[scanDesc].current->value1);
    if(scans[scanDesc].current->value2 != NULL)
	   free(scans[scanDesc].current->value2);
	free(scans[scanDesc].current);

	return AME_OK;
}


void AM_PrintError(char *errString) {
	printf("%s", errString);
	switch (AM_errno) {
		case AME_WRONG_TYPES:
			printf("Wrong Data-types Sizes! \n" );
			break;
		case AME_FILEEXISTS:
			printf("File Exists!\n" );
			break;
		case AME_FILEOPEN:
			printf("File is open!\n");
			break;
		case AME_CANTDESTROY:
			printf("Remove Failed!\n" );
			break;
		case AME_MAXSCANS:
			printf("Max number of scans are already open\n");
			break;
		case AME_SCANOPEN:
			printf("Scan is open!\n" );
			break;
		case AME_FILENOTOPEN:
			printf("File is not open!\n");
			break;

	}

	AM_errno = AME_OK;
}

void AM_Close() {
		BF_Block_Destroy(&block);
		BF_Block_Destroy(&temp_block);
		BF_Block_Destroy(&new_block);
		BF_Block_Destroy(&new_root);

		BF_Close();
}
