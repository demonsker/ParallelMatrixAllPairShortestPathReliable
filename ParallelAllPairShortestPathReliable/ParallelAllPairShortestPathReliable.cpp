#include "stdafx.h"
#include <mpi.h>
#include <stdlib.h>
#include <string.h>

#define PATH "C:\\Users\\Eucliwood\\Desktop\\stat(SaveMode)\\Parallel\\round1\\"
#define INF 999999
#define SIZE 8

void distance_generate(int[][SIZE]);
void distance_useexample(int[][SIZE]);
void find_AllPairShortestPath(int***, int***, int[SIZE], int);
int get_datasize_per_process(int);
int get_beginindex_frominput(int);
void process_print(int**, int);
void array_print(int**);
void array_copy(int**, int**, int);
void fix_path(int***, int***, int[SIZE], int, int, int);
void log_save(float);

int world_size, world_rank;

int main(int argc, char** argv) {

	double start_1, end_1, start_2, end_2;

	// Initialize the MPI environment
	MPI_Init(NULL, NULL);

	//Before setup
	start_1 = MPI_Wtime();

	// Get the number of processes
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	// Get the rank of the process
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

	//Number of row per processor
	int row_per_process = get_datasize_per_process(world_rank);

	//create memory for receive distance , path
	int i, j;
	int ***part_of_distance, ***part_of_path;
	int *temp_of_distance;
	temp_of_distance = (int(*)) malloc(SIZE * sizeof(int));

	part_of_distance = (int***)malloc(SIZE * sizeof(int**));
	part_of_path = (int***)malloc(SIZE * sizeof(int**));

	for (int k = 0; k < SIZE; k++)
	{
		part_of_distance[k] = (int**)malloc(row_per_process * sizeof(int*));
		part_of_path[k] = (int**)malloc(row_per_process * sizeof(int*));
		for (int i = 0; i < row_per_process; i++)
		{
			part_of_distance[k][i] = (int*)malloc(SIZE * sizeof(int));
			part_of_path[k][i] = (int*)malloc(SIZE * sizeof(int));
		}
	}
	
	for (i = 0; i < row_per_process; i++)
		for (j = 0; j < SIZE; j++)
		{
			part_of_path[0][i][j] = j;
		}

	
	//Master
	if (world_rank == 0)
	{
		//declare input
		int(*distance)[SIZE];
		distance = (int(*)[SIZE]) malloc(SIZE * sizeof(int[SIZE]));

		//After setup
		end_1 = MPI_Wtime();

		//Generate data
		distance_useexample(distance);
		//distance_generate(distance);

		//Start calculate
		start_2 = MPI_Wtime();

		//Send data partition to another processor
		for (i = 1; i < world_size; i++)
		{
			int row_begin = get_beginindex_frominput(i);
			int number_of_row = get_datasize_per_process(i);
			//send first pass node
			MPI_Send(distance, SIZE, MPI_INT, i, 0, MPI_COMM_WORLD);
			//send parition data to another processor
			for (int t = row_begin; t < row_begin+number_of_row; t++)
				MPI_Send(distance[t], SIZE, MPI_INT, i, 0, MPI_COMM_WORLD);
		}

		//partition data for master 
		for (i = 0; i < row_per_process; i++)
			for (j = 0; j < SIZE; j++)
			{
				if (i == 0)
					temp_of_distance[j] = distance[0][j];
				part_of_distance[0][i][j] = distance[i][j];
			}
	}
	//Slave
	else
	{
		//receive data from master
		MPI_Recv(temp_of_distance, SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		for (int t = 0; t < row_per_process; t++)
			MPI_Recv(part_of_distance[0][t], SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	

	//Find shrotest path
	find_AllPairShortestPath(part_of_distance, part_of_path, temp_of_distance, row_per_process);
	//End calculate
	end_2 = MPI_Wtime(); 

	printf("Process %d : Distance\n", world_rank);
	process_print(part_of_distance[SIZE-1], row_per_process);

	printf("Process %d : Path\n", world_rank);
	process_print(part_of_path[SIZE-1], row_per_process);

	float diff = (float)(end_1 - start_1 + end_2 - start_2);
	printf("Time : %.10f\n", diff);

	//log_save(diff);

	MPI_Finalize();
}

void find_AllPairShortestPath(int ***part_of_distance, int ***part_of_path, int temp_of_distance[SIZE], int row_per_process)
{
	//get postion of data from input
	int row_begin = get_beginindex_frominput(world_rank);
	int row_end = row_begin + get_datasize_per_process(world_rank);

	//Loop when k < SIZE
	for (int k = 0; k < SIZE; k++)
	{
		//find min route  when pass node k
		for (int i = 0; i < row_per_process; i++)
		{
			for (int j = 0; j < SIZE; j++)
			{
				int new_weight = part_of_distance[k][i][k] + temp_of_distance[j];

				if (new_weight <  part_of_distance[k][i][j])
				{
					part_of_path[k][i][j] = part_of_path[k][i][k];
					part_of_distance[k][i][j] = new_weight;
				}
		
			}
		}

		if (k + 1 < SIZE)
		{
			array_copy(part_of_distance[k], part_of_distance[k + 1], row_per_process);
			array_copy(part_of_path[k], part_of_path[k + 1], row_per_process);
		}
		else  break;

		//Find processor that must send pass node data
		if ((k + 1) >= row_begin && (k + 1) < row_end)
		{
			// index of pass node data
			int index = k + 1 - row_begin;

			int  r;

			//send pass node data to another processor
			for (r = 0; r < world_size; r++)
				if (r != world_rank) //not send to itself
				{
					MPI_Send(part_of_distance[k][index], SIZE, MPI_INT, r, 0, MPI_COMM_WORLD);
				}

			//update pass node data self
			for (r = 0; r < SIZE; r++)
				temp_of_distance[r] = part_of_distance[k][index][r];
		}
		else
		{	//receive pass node data
			MPI_Recv(temp_of_distance, SIZE, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		
	}
}

void fix_path(int ***part_of_distance, int ***part_of_path, int temp_of_distance[SIZE], int row_per_process, int u, int v)
{/*
	//mark a broken edge 
	alldistance[0][u][v] = INF;

	//useless edge
	if (allpath[SIZE - 1][u][v] != v)
	{
		printf("This edge is useless\n");
		return;
	}

	//re-calculate : edge broke at node 0
	if (u == 0)
	{
		printf("Re-Calculate\n");
		find_AllPairShortestPath(alldistance, allpath);
		return;
	}

	//find new shortest path when pass k = 0 to k < u
	for (int k = 0; k < u; k++)
	{
		int new_weight = alldistance[k][u][k] + alldistance[k][k][v];

		if (new_weight < alldistance[k][u][v])
		{
			alldistance[k][u][v] = new_weight;
			allpath[k][u][v] = allpath[k][u][k];
		}
		alldistance[k + 1][u][v] = alldistance[k][u][v];
		allpath[k + 1][u][v] = allpath[k][u][v];
	}

	//Copy colum v from k = u-1 to u
	for (int i = 0; i < SIZE; i++)
	{
		alldistance[u][i][v] = alldistance[u - 1][i][v];
		allpath[u][i][v] = allpath[u - 1][i][v];
	}
	*/
	//Begin calculate at k = u
	//get postion of data from input
	int row_begin = get_beginindex_frominput(world_rank);
	int row_end = row_begin + get_datasize_per_process(world_rank);

	//Loop when k < SIZE
	for (int k = 0; k < SIZE; k++)
	{
		//find min route  when pass node k
		for (int i = 0; i < row_per_process; i++)
		{
			for (int j = 0; j < SIZE; j++)
			{
				int new_weight = part_of_distance[k][i][k] + temp_of_distance[j];

				if (new_weight <  part_of_distance[k][i][j])
				{
					part_of_path[k][i][j] = part_of_path[k][i][k];
					part_of_distance[k][i][j] = new_weight;
				}

			}
		}

		if (k + 1 < SIZE)
		{
			array_copy(part_of_distance[k], part_of_distance[k + 1], row_per_process);
			array_copy(part_of_path[k], part_of_path[k + 1], row_per_process);
		}
		else  break;

		//Find processor that must send pass node data
		if ((k + 1) >= row_begin && (k + 1) < row_end)
		{
			// index of pass node data
			int index = k + 1 - row_begin;

			int  r;

			//send pass node data to another processor
			for (r = 0; r < world_size; r++)
				if (r != world_rank) //not send to itself
				{
					MPI_Send(part_of_distance[k][index], SIZE, MPI_INT, r, 0, MPI_COMM_WORLD);
				}

			//update pass node data self
			for (r = 0; r < SIZE; r++)
				temp_of_distance[r] = part_of_distance[k][index][r];
		}
		else
		{	//receive pass node data
			MPI_Recv(temp_of_distance, SIZE, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}

	}
}

void distance_generate(int data[][SIZE])
{
	int i, j, r;

	for (i = 0; i < SIZE; i++)
	{
		data[i][i] = 0;
		for (j = i + 1; j < SIZE; j++)
		{
			r = (rand() % 20) + 1;
			if (r == 19)
				data[i][j] = INF;
			else
				data[i][j] = r;
			data[j][i] = data[i][j];
		}
	}
}

void process_print(int **distance, int n)
{
	for (int i = 0; i < n; i++)
	{
		for (int j = 0; j < SIZE; j++)
		{
			if (distance[i][j] == INF)
				printf("%7s", "INF");
			else
				printf("%7d", distance[i][j]);
		}

		printf("\n");
	}
}

void array_copy(int **sour, int **dest, int n)
{
	for (int i = 0; i < n; i++)
	{
		for (int j = 0; j < SIZE; j++)
		{
			dest[i][j] = sour[i][j];
		}
	}
}

void array_print(int **data)
{
	for (int i = 0; i < SIZE; i++)
	{
		for (int j = 0; j < SIZE; j++)
		{
			if (data[i][j] == INF)
				printf("%7s", "INF");
			else
				printf("%7d", data[i][j]);
		}
		printf("\n");
	}
}

int get_datasize_per_process(int rank)
{
	int n = SIZE / world_size;
	int m = SIZE % world_size;
	if (m != 0)
		if (rank < m)
			n++;
	return n;
}

int get_beginindex_frominput(int world_rank)
{
	if (world_rank == 0) return 0;
	int begin, end, np, i;
	end = get_datasize_per_process(0);
	for (i = 1; i < world_size; i++)
	{
		np = get_datasize_per_process(i);
		begin = end;
		end = begin + np;
		if (world_rank == i)
			return begin;
	}
	return -1;
}

void distance_useexample(int data[][SIZE])
{
	int i, j;

	int example[8][8] = {
		{ 0,1,9,3,INF,INF,INF,INF },
		{ 1,0,INF,1,INF,3,INF,INF },
		{ 9,INF,0,INF,INF,3,10,INF },
		{ 3,1,INF,0,5,INF,INF,8 },
		{ INF,INF,INF,5,0,2,2,1 },
		{ INF,3,3,INF,2,0,INF,INF },
		{ INF,INF,10,INF,2,INF,0,4 },
		{ INF,INF,INF,8,1,INF,4,0 }
	};

	for (i = 0; i < SIZE; i++)
	{
		for (j = 0; j < SIZE; j++)
		{
			data[i][j] = example[i][j];
		}
	}
}

void log_save(float diff)
{
	int i;
	if (world_rank > 0)
		MPI_Send(&diff, 1, MPI_FLOAT, 0, 0, MPI_COMM_WORLD);
	else
	{
		float temp;
		for (i = 1; i < world_size; i++)
		{
			MPI_Recv(&temp, 1, MPI_FLOAT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			if (temp > diff)
				diff = temp;
		}
		printf("Finish Time : %f\n", diff);

		FILE * fp;
		char fileName[10];
		char filePath[70] = PATH;

		sprintf(fileName, "%d.txt", world_size);
		strcat(filePath, fileName);
		fp = fopen(filePath, "a");
		fprintf(fp, "%d,%.10f\n", SIZE, diff);
		fclose(fp);
	}
}
