/*
Matrix-matrix multiplication, Fox
Data distribution of matrix: checkerboard
*/

#include<stdio.h>
#include<mpi.h>
#include<string.h>
#include<stdlib.h>

typedef int dtype;					// Change these two definitions when the matrix and vector element types change
#define MPI_TYPE MPI_INT
#define BLOCK_OWNER(j,p,n) (((p)*((j)+1)-1)/(n))
#define BLOCK_LOW(id,p,n)  ((id)*(n)/(p))						// low_value= (in/p)
#define BLOCK_HIGH(id,p,n) (BLOCK_LOW((id)+1,p,n)-1)					// high value (i+1)n/p-1
#define BLOCK_SIZE(id,p,n) (BLOCK_HIGH(id,p,n)-BLOCK_LOW(id,p,n)+1)			// size of the interval
#define PTR_SIZE           (sizeof(void*))						// size of a pointer

void allocate_matrix ( dtype ***a, int r, int c )
{	dtype *local_a = (void *)malloc(r*c*sizeof(dtype));
	(*a) = (void *)malloc(r*PTR_SIZE);
	dtype *l= &local_a[0];
	int i;
	for (i=0; i<r; i++ )
	{	(*a)[i]= l;
		l += c;
	}
}

void read_checkerboard_matrix ( dtype ***a, MPI_Datatype dtype, int n, MPI_Comm grid_comm, int * grid_size)
{	int coords[2], dest_id, grid_coord[2], grid_id, i, j, k, p, i1;
	MPI_Status status;
	MPI_Comm_rank (grid_comm, &grid_id);
	MPI_Comm_size (grid_comm, &p);
	int *buffer, *raddr;
	int local_cols= BLOCK_SIZE(grid_coord[1],grid_size[1],n);
	
	if (grid_id == 0)
		buffer = malloc (n * sizeof(dtype));
	MPI_Barrier(grid_comm);

	// first read and distribute matrix a and then again read and distribute matrix b
	for (i = 0; i < grid_size[0]; i++)
	{	coords[0] = i;
		for (j = 0; j < BLOCK_SIZE(i,grid_size[0],n); j++)
		{	if (grid_id == 0)
			{	for ( i1=0; i1<n; i1++ )
					printf("%d\t", buffer[i1]=rand()%50);
				printf("\n");
			}
			
			for (k = 0; k < grid_size[1]; k++)
			{	coords[1] = k;
				raddr = buffer + BLOCK_LOW(k,grid_size[1],n);
				MPI_Cart_rank (grid_comm, coords, &dest_id);
				if (grid_id == 0)
				{	if (dest_id == 0)
						memcpy ((*a)[j], raddr, local_cols * sizeof(dtype));
					else
						MPI_Send (raddr, BLOCK_SIZE(k,grid_size[1],n), dtype, dest_id, 0, grid_comm);
				}
				else if (grid_id == dest_id)
					MPI_Recv ((*a)[j], local_cols, dtype, 0, 0, grid_comm, &status);
			}
		}
	}
	if (grid_id == 0)
		free (buffer);
}


void set_to_zero( dtype ***a, int r, int c )
{	int i,j;
	for ( i = 0; i < r; i++)
		for (j = 0; j < c; j++)
			(*a)[i][j]=0;
}

// must be squate matrix only. p is square number and n/sqrt(p) == 0
void local_matrix_multiply( dtype ***a, dtype*** b, dtype*** c, int row, int col)
{	int i,j,k;
	for ( i = 0; i < row; i++)
	{	for ( j = 0; j < col; j++)
		{	for ( k = 0; k < row; k++)
				(*c)[i][j] += ( (*a)[i][k] * (*b)[k][j] );
		}
	}
}

void fox( dtype*** local_a, dtype*** local_b, dtype*** local_c, MPI_Comm col_comm, MPI_Comm row_comm, int *grid_size, int *grid_coords, int local_rows, int local_cols )
{	int my_rank, source, dest, i, j, stage, bcast_root;
	dtype *local_tmp, *l;
	MPI_Datatype matrix_type;
	MPI_Comm_rank (row_comm, &my_rank);
	MPI_Status status;
	source = (grid_coords[0] + 1) % grid_size[0];			// Calculate addresses for circular shift of B
	dest = (grid_coords[0] + grid_size[0] - 1) % grid_size[0];
	/* Set aside storage for the broadcast block of A */
	local_tmp = (dtype *) malloc ( (size_t) local_cols* local_rows * sizeof(dtype));
	dtype **temp = (dtype **) malloc ( (size_t) local_rows * PTR_SIZE);
	l= &local_tmp[0];
	for (i=0; i<local_rows; i++ )
	{	temp[i]= l;
		l += local_cols;
	}
	
	for (stage = 0; stage < grid_size[0]; stage++)
	{	bcast_root = (grid_coords[0] + stage) % grid_size[0];
		int grd_coord[2]={grid_coords[0], bcast_root}, row_rank;
		MPI_Cart_rank(row_comm, grd_coord, &row_rank);
		if (bcast_root == grid_coords[1])
		{	// refer to ./pointer_malloc_reference.c for understanding the concept and ./pass_2d_dynamic_matrix_bcast.c for how 2 pass dynamically allocated matrix
			// we pass the starting address of the continous blocks of array
			MPI_Bcast(&((*local_a)[0][0]), local_rows*local_cols, MPI_TYPE, bcast_root, row_comm);	// column number is the rank
			local_matrix_multiply(local_a, local_b, local_c, local_rows, local_cols);
		}
		else
		{	// refer to ./pointer_malloc_reference.c for understanding the concept
			// we pass the starting address of the continous blocks of array
			MPI_Bcast(&(temp[0][0]),  local_rows*local_cols, MPI_TYPE, bcast_root, row_comm);
			local_matrix_multiply(&temp, local_b, local_c, local_rows, local_cols);
		}
		MPI_Sendrecv_replace(&((*local_b)[0][0]),  local_rows*local_cols, MPI_TYPE, dest, 0, source, 0, col_comm, &status);
		// we'll have to create new if not s square matrix
	}
	free(local_tmp);
}

void print_checkerboard_matrix ( dtype **c, MPI_Comm comm, int n, int local_cols, int local_rows, int *grid_size)
{	int mat_row, mat_col, grid_row, grid_col, source, coords[2], my_rank, i, j, k;
	MPI_Comm_rank(comm, &my_rank);
	dtype *temp= malloc(n*sizeof(dtype));
	MPI_Status status;
	if (my_rank == 0)
	{	for (i = 0; i < n; i++)
		{	coords[0] = BLOCK_OWNER(i,grid_size[0],n);
			for (j = 0; j < grid_size[1]; j++)
			{	coords[1] = j;
				MPI_Cart_rank(comm, coords, &source);
				if (source == 0)
				{	for(k = 0; k < local_cols; k++)
						printf("%d\t", c[i][k]);
				}
				else
				{	MPI_Recv(temp, local_cols, MPI_TYPE, source, 0, comm, &status);
					for(k = 0; k < local_cols; k++)
						printf("%d\t", temp[k]);
				}
			}
			printf("\n");
		}
	}
	else
	{	for (i = 0; i < local_rows; i++) 
			MPI_Send(&(c[i][0]), local_cols, MPI_TYPE, 0, 0, comm);
	}
	free(temp);
}

int main (int argc, char *argv[])
{	dtype **a, **b, **c;
	double max_seconds, seconds;
	int grid_id, my_rank, i, j, n, p, grid_size[2], grid_coords[2], coords[2], periodic[2], grid_period[2], free_coords[2], local_cols, local_rows;
	
	MPI_Comm grid_comm;
	MPI_Comm row_comm;
	MPI_Comm col_comm;
	MPI_Status status;
	MPI_Init (&argc, &argv);
	MPI_Comm_rank (MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size (MPI_COMM_WORLD, &p);
	grid_size[0] = grid_size[1] = 0;
	MPI_Dims_create (p, 2, grid_size);
	periodic[0] = periodic[1] = 1;					// for MPI_send_recv_replace(). Process 0-1 == n-1 periodicity
	MPI_Cart_create (MPI_COMM_WORLD, 2, grid_size, periodic, 1, &grid_comm);
	MPI_Comm_rank (grid_comm, &grid_id);						// get_rank in the communicator after the shuffle
	MPI_Cart_coords (grid_comm, grid_id, 2, grid_coords);				// coordinate in the topology
	free_coords[0]=0, free_coords[1]=1;
	MPI_Cart_sub(grid_comm, free_coords, &row_comm);
	free_coords[0]=1, free_coords[1]=0;
	MPI_Cart_sub(grid_comm, free_coords, &col_comm);
	
	if ( grid_id == 0)
	{	printf("Enter dimension of matrix: ");
		scanf("%d",&n);
	}
	MPI_Bcast (&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
	// we take only square matrices and hence local_rows = local_cols here
	local_rows = BLOCK_SIZE(grid_coords[0],grid_size[0],n);
	local_cols = BLOCK_SIZE(grid_coords[1],grid_size[1],n);
	allocate_matrix(&a, local_rows, local_cols);
	allocate_matrix(&b, local_rows, local_cols);
	allocate_matrix(&c, local_rows, local_cols);
	set_to_zero(&c, local_rows, local_cols);
	MPI_Barrier(grid_comm);

	read_checkerboard_matrix ( &a, MPI_TYPE, n, grid_comm, grid_size); 	// done local distribution of matrices
	read_checkerboard_matrix ( &b, MPI_TYPE, n, grid_comm, grid_size);
	MPI_Barrier(MPI_COMM_WORLD);
	fox( &a, &b, &c, col_comm, row_comm, grid_size, grid_coords, local_rows, local_cols );
	free(a);
	free(b);
	MPI_Barrier(MPI_COMM_WORLD);
	print_checkerboard_matrix ( c, grid_comm, n, local_rows, local_cols, grid_size); 	// done local distribution of matrices
	free(c);
	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();
	return 0;
}
