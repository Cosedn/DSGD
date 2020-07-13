#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include "dsgd.h"
#include "mpi.h"

int main(int argc, char *argv[])
{
	FILE *record;
	int i,j,k;
	int t,tg;
	int schedule;
	RatingMatrix *rm_buffer;/* rm_buffer is part of the matirx R, containing all the ratings
				 whose row indices are in the range of u_buffer. */
	double *u_buffer,	/* u_buffer is part of the matrix U arranged to local task. */
	       *v_buffer[ROW],	/* v_buffer[i] is the i-th part of the matrix V. */
	       *r_buffer;	/* rm_buffer is the target matrix, r_buffer is the matrix to be
				 trained. When each iteration completes we need to compute the
				 distance between rm_buffer and r_buffer to determine whether
				 training is over.*/
	Block r_tag[ROW*COL],rl_tag[ROW*COL];
	int r_list[ROW*COL];
	int attr=ATTR;
	int rm_size=0,r_size=ROW*COL,rl_size=ROW;
	int u_entries[USERS]={0},v_entries[ITEMS]={0};
	int u_block[ROW+1]={0},v_block[COL+1]={0};
	int u_bsize[ROW]={0},v_bsize[COL]={0};
	double aver;
	double rsize_array[ROW]={0};
	
	int loop_continue,loop_counts;
	double sub_rmse,curr_rmse,rmse=MAXIMUM;
	double rmse_array[ROW]={0};
	int num;
	int begin,end;
	struct timeval tv1,tv2;

	double e;
	double alpha=0.00005;
	double lmd1=0.05;
	double lmd2=0.05;
	double temp;
	double t0;
	int r0,c0,r1,c1;
	
	MPI_Status status[ROW];
	MPI_Status status1[ROW];
	MPI_Request request[ROW];
	MPI_Request request1[ROW];
	int myid,numprocs;
	
	for(i=0;i<r_size;i++) 
	{
		r_tag[i].row=i/COL;
		r_tag[i].col=i-i/COL*COL;
		r_tag[i].entries=0;
	}
	
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	if(myid==0)
	{
//		printf("%s\n", STR2(DATASET_PATH));//
		num=Input_File(STR2(DATASET_PATH), u_entries, v_entries);
		aver=num/ROW;
		Make_Block(u_entries, v_entries, u_block, v_block , USERS, ITEMS, aver);
/*		printf("%d\n",aver);
		for(i=0;i<=ROW;i++) printf("%d ",u_block[i]);
		printf("\n");
		for(i=0;i<=COL;i++) printf("%d ",v_block[i]);
		printf("\n");*/
/*		for(i=0;i<ROW;i++)
		{
			t=0;
			for(j=u_block[i];j<u_block[i+1];j++) t+=u_entries[j];
			printf("%d ",t);
		}
		printf("\n");
		for(i=0;i<COL;i++)
		{
			t=0;
			for(j=v_block[i];j<v_block[i+1];j++) t+=v_entries[j];
			printf("%d ",t);
		}
		printf("\n");*/
		File_Separate(STR2(DATASET_PATH), u_block, v_block, r_tag); //store r_tag into a file meanwhile
		printf("File Separate Finish!\n");
		
		for(i=0;i<ROW;i++) u_bsize[i]=u_block[i+1]-u_block[i];
		for(i=0;i<COL;i++) v_bsize[i]=v_block[i+1]-v_block[i];
		Get_Rtag(r_tag);
//		for(i=0;i<r_size;i++) printf("%d %d %d\n",r_tag[i].row,r_tag[i].col,r_tag[i].entries);
/*		for(i=0;i<ROW;i++) printf("%d ",u_bsize[i]);
		printf("\n");
		for(i=0;i<COL;i++) printf("%d ",v_bsize[i]);
		printf("\n");*/
		
		MPI_Bcast(r_tag, r_size*sizeof(Block), MPI_BYTE, 0, MPI_COMM_WORLD);
		MPI_Bcast(u_bsize, ROW, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(v_bsize, COL, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(u_block, ROW+1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(v_block, COL+1, MPI_INT, 0, MPI_COMM_WORLD);
		if((record=fopen("record.txt","w"))==NULL)
		{
			printf("cannot write to this file!\n");
			exit(0);
		}
		for(i=0;i<ROW;i++)
			for(j=0;j<COL;j++) rsize_array[i]+=r_tag[i*COL+j].entries;
	}
	else
	{
		MPI_Bcast(r_tag, r_size*sizeof(Block), MPI_BYTE, 0, MPI_COMM_WORLD);
		MPI_Bcast(u_bsize, ROW, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(v_bsize, COL, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(u_block, ROW+1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(v_block, COL+1, MPI_INT, 0, MPI_COMM_WORLD);
		for(i=0;i<rl_size;i++) rl_tag[i]=r_tag[(myid-1)*COL+i];//
		for(i=0;i<rl_size;i++)
		{
			r_list[i]=rm_size;
			rm_size+=rl_tag[i].entries;
//			if(myid==1) printf("%d\n", r_list[i]);
		}
		r_list[rl_size]=rm_size;
//		printf("%d\n",rm_size);
		rm_buffer=(RatingMatrix *)calloc(rm_size, sizeof(RatingMatrix));
		r_buffer=(double *)calloc(rm_size, sizeof(double));
//		printf("myid is %d:\n",myid);
//		for(i=0;i<rl_size;i++) printf("%d  %d  %d\n",rl_tag[i].row,rl_tag[i].col,rl_tag[i].entries);
		File_Block_Get(rl_tag, rl_size, rm_buffer);
//		printf("%d %d\n",row_number,col_number);
//		if(myid==1) for(i=0;i<rl_size;i++) printf("%d %d %d\n",rl_tag[i].row,rl_tag[i].col,rl_tag[i].entries);

		u_buffer=(double *)calloc(u_bsize[myid-1]*attr,sizeof(double));
		for(i=0;i<COL;i++) v_buffer[i]=(double *)calloc(v_bsize[i]*attr,sizeof(double));
		for(j=0;j<u_bsize[myid-1]*attr;j++) u_buffer[j]=1;
		for(i=0;i<COL;i++)
			for(j=0;j<v_bsize[i]*attr;j++) v_buffer[i][j]=1;
		for(i=0;i<rl_size;i++)
		{
			begin=r_list[i];
			end=r_list[i+1];
			for(j=begin;j<end;j++)
			{
				r0=rm_buffer[j].user-u_block[myid-1];
				c0=rm_buffer[j].item-v_block[i];
				r_buffer[j]=0;
				for(k=0;k<attr;k++) r_buffer[j]+=u_buffer[r0*attr+k]*v_buffer[i][c0*attr+k];
			}
		}
//		printf("%d:%d\n",myid,u_bsize[myid-1]);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	
	if(myid==0)
	{
		loop_counts=0;
		gettimeofday(&tv1,NULL);
		while(1)
		{
			loop_continue=0;
			sub_rmse=0;
			curr_rmse=0;
			schedule=0;
			MPI_Bcast(&loop_continue, 1, MPI_INT, 0, MPI_COMM_WORLD);
			MPI_Bcast(&schedule, 1, MPI_INT, 0, MPI_COMM_WORLD);
			for(i=0;i<ROW;i++) MPI_Irecv(&rmse_array[i], 1, MPI_DOUBLE, i+1, i+201, MPI_COMM_WORLD, &request1[i]);
			for(i=0;i<ROW;i++) MPI_Wait(&request1[i], &status1[i]);
			for(i=0;i<ROW;i++)
			{
			//	printf("%d %.4f\n",i,rmse_array[i]);
				curr_rmse+=rmse_array[i]/num*rsize_array[i];
			}
			curr_rmse=sqrt(curr_rmse);
			fprintf(record,"%.4f\n",curr_rmse);
			if(curr_rmse<rmse && loop_counts< MAX_ITER ) rmse=curr_rmse;//
			else
			{
				loop_continue=1;
				MPI_Bcast(&loop_continue, 1, MPI_INT, 0, MPI_COMM_WORLD);
				break;
			}
			loop_counts++;
		}
		gettimeofday(&tv2,NULL);
		fprintf(record, "loop counts:%d\n", loop_counts);
		fprintf(record, "average time per loop:%.4fms\n",((double)(tv2.tv_sec-tv1.tv_sec)*1000+(double)(tv2.tv_usec-tv1.tv_usec)/1000)/loop_counts);
		fclose(record);
	}
	else
	{
		while(1)
		{
			MPI_Bcast(&loop_continue, 1, MPI_INT, 0, MPI_COMM_WORLD);
			if(loop_continue==1) break;
			MPI_Bcast(&schedule, 1, MPI_INT, 0, MPI_COMM_WORLD);
			t=(schedule+myid-1)%(numprocs-1);//this is important!!
//			printf("myid:%d t:%d\n",myid,t);//
			for(i=0;i<rl_size;i++)
			{
				tg=(t+i)%rl_size;
//				printf("%d\n",tg);//
				MPI_Irecv(v_buffer[(tg+1)%rl_size], v_bsize[(tg+1)%rl_size]*attr, MPI_DOUBLE, (myid+numprocs-1)%(numprocs-1)+1, (tg+1)%rl_size, MPI_COMM_WORLD, &request[(tg+1)%rl_size]);
				begin=r_list[tg];
				end=r_list[tg+1];
				for(j=begin;j<end;j++)
				{
					r0=rm_buffer[j].user-u_block[myid-1];
					c0=rm_buffer[j].item-v_block[tg];
					e=rm_buffer[j].rating-r_buffer[j]; //r_buffer is updated when compute RMSE
					for(k=0;k<attr;k++)
					{
						r1=r0*attr+k;
						c1=c0*attr+k;
						temp=v_buffer[tg][c1];
						v_buffer[tg][c1]+=alpha*(e*u_buffer[r1]-lmd2*temp);
						u_buffer[r1]+=alpha*(e*temp-lmd1*u_buffer[r1]);
					}
				}
				MPI_Send(v_buffer[tg], v_bsize[tg]*attr, MPI_DOUBLE, (myid+numprocs-3)%(numprocs-1)+1, tg, MPI_COMM_WORLD);
				MPI_Wait(&request[(tg+1)%rl_size], &status[(tg+1)%rl_size]);
			}
			
			sub_rmse=0;
			for(i=0;i<rl_size;i++)
			{
				begin=r_list[i];
				end=r_list[i+1];
				for(j=begin;j<end;j++)
				{
					r0=rm_buffer[j].user-u_block[myid-1];
					c0=rm_buffer[j].item-v_block[i];
					r_buffer[j]=0;
//					if(u_buffer[r0*attr+k]!=1.0) printf("%.4f\n",u_buffer[r0*attr+k]);//
					for(k=0;k<attr;k++) r_buffer[j]+=u_buffer[r0*attr+k]*v_buffer[i][c0*attr+k];
					t0=rm_buffer[j].rating-r_buffer[j];
					sub_rmse+=t0*t0/rm_size;
				}
//				printf(" %.4f\n",sub_rmse);//
			}
			MPI_Isend(&sub_rmse, 1, MPI_DOUBLE, 0, myid+200, MPI_COMM_WORLD, &request1[myid-1]);
//			printf("%d:%.4f\n",myid,sub_rmse);
		}
	}

	MPI_Finalize();
	
	if(myid!=0)
	{
		free(u_buffer);
		for(i=0;i<COL;i++) free(v_buffer[i]);
		free(rm_buffer);
		free(r_buffer);
	}
	
	return 0;
}
