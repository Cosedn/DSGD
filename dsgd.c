#include <stdio.h>
#include <stdlib.h>
#include "dsgd.h"

typedef struct _Head
{
	struct _Node *node;
	struct _Node *current;
}Head;

typedef struct _Node
{
	RatingMatrix buffer[BUF];
	int size;
	struct _Node *next;
}Node;

void Init_List(Head *head)
{
	Node *node;
	node = (Node *)calloc(1, sizeof(Node));
	node->size = 0;
	node->next = NULL;
	head->node = node;
	head->current = node;
}

void Add_To_List(Head *head, RatingMatrix *rm)
{
	if(head->current->size >= BUF - 1)
	{
		Node *node = NULL;
		if((node = (Node *)calloc(1, sizeof(Node))) == NULL)
		{
			printf("memory allocation failed!\n");
			exit(0);
		}
		node->size = 0;
		node->next = NULL;
		head->current->next = node;
		head->current = head->current->next;

		head->current->buffer[head->current->size] = *rm;
		head->current->size++;
	}
	else
	{
		head->current->buffer[head->current->size] = *rm;
		head->current->size++;
	}
}

void Destroy_List(Head *head)
{
	Node *temp;
	head->current = head->node;
	while(head->current != NULL)
	{
		temp = head->current->next;
		free(head->current);
		head->current = temp;
	}
}

/*void itoa(int num, char* str)
{
        int i=0,j;
        int t;
        char str1[10];
        while(1)
        {
                if(num==0) break;
                t=num-num/10*10;
                num=(num-t)/10;
                str1[i]=(char)(t+48);
                i++;
        }
        for(j=0;j<i;j++) str[j]=str1[i-j-1];
        str[j]='\0';
}*/

int Input_File(char* str, int *u_entries, int *v_entries)
{
	FILE *fp;
	int user,item;
	double rating;
	long temp;
	int num=0;
	if((fp=fopen(str,"r"))==NULL)
	{
		printf("cannot open this file!\n");
		exit(0);
	}
    while(fscanf(fp,"%d %d %lf",&user,&item,&rating)!=EOF)
	{
		u_entries[user-1]++;
		v_entries[item-1]++;
		num++;
	}
	fclose(fp);
	return num;
}

#ifdef ALGO_BAPA
void Make_Block(int *u_entries, int *v_entries, int *u_block, int *v_block, int user, int item, double aver)
{
/*	int i;
        double row_number,col_number;
        row_number=(double)user/ROW;
        col_number=(double)item/COL;
        u_block[0]=0;
        v_block[0]=0;
        u_block[ROW]=user;
        v_block[COL]=item;
        for(i=1;i<ROW;i++) u_block[i]=(int)(i*row_number);
        for(i=1;i<COL;i++) v_block[i]=(int)(i*col_number);*/
	int i, j;
	double t0, t1, sum, aver_adder;
	u_block[0] = 0;
	i = 1;
	j = 0;
	sum = 0;
	t0 = MAXIMUM;
	aver_adder = aver;
	while(j < user)
	{
		sum += u_entries[j];
		t1 = (sum > aver_adder)?(sum - aver_adder):(aver_adder - sum);
		if(t1 <= t0)
		{
			t0 = t1;
			j++;
		}
		else
		{
			u_block[i] = j;
			sum -= u_entries[j];
			i++;
			t0 = MAXIMUM;
			aver_adder += aver;
		}
		if(i == ROW)
		{
			u_block[i] = user;
			break;
		}
	}
	v_block[0] = 0;
	i = 1;
	j = 0;
	sum = 0;
	t0 = MAXIMUM;
	aver_adder = aver;
	while(j < item)
	{
		sum += v_entries[j];
		t1 = (sum > aver_adder)?(sum - aver_adder):(aver_adder - sum);
		if(t1 <= t0)
		{
			t0 = t1;
			j++;
		}
		else
		{
			v_block[i] = j;
			sum -= v_entries[j];
			i++;
			t0 = MAXIMUM;
			aver_adder += aver;
		}
		if(i == COL)
		{
			v_block[i] = item;
			break;
		}
		//printf("%d %d %d %.2f %.2f\n", j, v_entries[j], i, sum, aver_adder);
	}
}
#endif

#ifdef ALGO_ESPA
void Make_Block(int *u_entries, int *v_entries, int *u_block, int *v_block, int user, int item, double aver)
{
	int i;
	double row_number,col_number;
	row_number=(double)user/ROW;
	col_number=(double)item/COL;
	u_block[0]=0;
	v_block[0]=0;
	u_block[ROW]=user;
	v_block[COL]=item;
	for(i=1;i<ROW;i++) u_block[i]=(int)(i*row_number);
	for(i=1;i<COL;i++) v_block[i]=(int)(i*col_number);
}
#endif

void File_Block_Get(Block *rl_tag, int rl_size, RatingMatrix *rm_buffer) //Get the corresponding blocks in files for each thread
{
	FILE *fp;
	int i;
	int sz=0;
	int size=0;
	int get_block=0;
	int num_block=0;
	int match;
	RatingMatrix temp;

	if((fp=fopen("blockfile","r"))==NULL)
	{
		printf("cannot open this file!\n");
		exit(0);
	}

	//for(i=0;i<rl_size;i++)
	fscanf(fp, "%d", &match);
	if(match != ROW * COL)
	{
		printf("This file does not match the program!\n");
		exit(0);
	}

	while(get_block < rl_size)
	{
		if(fscanf(fp, "%d", &size) == EOF) break;
		if(rl_tag[get_block].row * COL + rl_tag[get_block].col == num_block)
		{
			for(i = 0; i < size; i++)
			{
				fscanf(fp, "%d %d %lf", &rm_buffer[sz].user, &rm_buffer[sz].item, &rm_buffer[sz].rating);
				sz++;
			}
			num_block++;
			get_block++;
		}
		else
		{
			for(i = 0; i < size; i++)
				fscanf(fp, "%d %d %lf", &temp.user, &temp.item, &temp.rating);
			num_block++;
		}
	}
	fclose(fp);
}

void File_Separate(char *str, int *u_block, int *v_block, Block *r_tag) //Separate the Rating Matrix into blocks and store them in files
{
	FILE *fp,*fq;
	int i,j,k;
	int r,c,p;
	int user,item;
	double rating;
	int size;
	RatingMatrix rm;

	Head head[ROW][COL];
	int num[ROW][COL];

	if((fp=fopen(str,"r"))==NULL)
	{
		printf("cannot open this file!\n");
		exit(0);
	}
	size=ROW*COL;
	if((fq=fopen("blockfile","w"))==NULL)
	{
		printf("cannot write to this file!\n");
		exit(0);
	}

	for(i = 0; i < ROW; i++)
                for(j = 0; j < COL; j++) Init_List(&head[i][j]);

    	while(fscanf(fp,"%d %d %lf",&user,&item,&rating)!=EOF)
	{
		r=user-1;
		c=item-1;
		for(i=0;i<=ROW;i++) if(r<u_block[i]) break;
		for(j=0;j<=COL;j++) if(c<v_block[j]) break;
		p=(i-1)*COL+j-1;
		//fprintf(fq[p],"%d    %d    %d\n",r,c,rating);
		rm.user = r;
		rm.item = c;
		rm.rating = rating;
		Add_To_List(&head[i-1][j-1], &rm);
		r_tag[p].entries++;
	}

	for(i = 0; i < ROW; i++)
		for(j = 0; j < COL; j++)
		{
			num[i][j] = 0;
			head[i][j].current = head[i][j].node;
			while(head[i][j].current != NULL)
			{
				num[i][j] += head[i][j].current->size;
				head[i][j].current = head[i][j].current->next;
			}
		}

	fprintf(fq, "%d\n", size);
	for(i = 0; i < ROW; i++)
		for(j = 0; j < COL; j++)
		{
			fprintf(fq, "%d\n", num[i][j]);
			head[i][j].current = head[i][j].node;
			while(head[i][j].current != NULL)
			{
				for(k = 0; k < head[i][j].current->size; k++)
					fprintf(fq,"%d    %d    %.2f\n",
						head[i][j].current->buffer[k].user,
						head[i][j].current->buffer[k].item,
						head[i][j].current->buffer[k].rating);
				head[i][j].current = head[i][j].current->next;
			}
		}

	Store_Rtag(r_tag);
	fclose(fp);
	fclose(fq);
	for(i = 0; i < ROW; i++)
		for(j = 0; j < COL; j++) Destroy_List(&head[i][j]);
}

void Store_Rtag(Block *r_tag)
{
	FILE *fp=NULL;
	int size=ROW*COL;
	int i;
	if((fp=fopen("rtag.txt","w"))==NULL)
	{
		printf("cannot write to this file!\n");
		exit(0);
	}
	for(i=0;i<size;i++) fprintf(fp,"%d %d %d\n",r_tag[i].row,r_tag[i].col,r_tag[i].entries);
	fclose(fp);
}

void Get_Rtag(Block *r_tag)
{
	FILE *fp=NULL;
	int size=ROW*COL;
	int i;
	if((fp=fopen("rtag.txt","r"))!=NULL)
	{
		for(i=0;i<size;i++) fscanf(fp,"%d %d %d",&r_tag[i].row,&r_tag[i].col,&r_tag[i].entries);
	}
	fclose(fp);
}
