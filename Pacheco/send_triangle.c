/* send_triangle.c -- send the upper triangle of a matrix from process 0
 *     to process 1 
 *
 * Input:  None
 * Output:  The matrix received by process 1
 *
 * Note:  This program should only be run with 2 processes.
 *
 * See Chap 6, p. 98, in PPMPI.
 */
#include <stdio.h>
#include "mpi.h"
#define n 10

int main( int argc, char* argv[] )
{	int p, my_rank;
	float A[n][n];          /* Complete Matrix */
	float T[n][n];          /* Upper Triangle  */
	int displacements[n], block_lengths[n];
	MPI_Datatype  index_mpi_t;
	int i, j;
	MPI_Status status;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &p);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	for (i = 0; i < n; i++)
	{	block_lengths[i] = n-i;				// length 1->n
		displacements[i] = (n+1)*i;			// from base address!!
	}
	/*
	Hence we get a data type NOT signature of form:
	(n*MPI_Float,0), ((n-1)*MPI_Float, (n+1)*1*sizeof(MPI_Float)), ...... (n+1)*(n-1)*MPI_Float, n*i*sizeof(MPI_Float))-> hence our signature. (n-i)*MPI_Floatv as no.of elements in each block = n-i for each copy
	*/
	MPI_Type_indexed(n, block_lengths, displacements, MPI_FLOAT, &index_mpi_t);
	// for all derived type structures
	// Sending upper triangle- INDEX
	// Sending lower triangle- VECTOR
	MPI_Type_commit(&index_mpi_t);
	if (my_rank == 0)
	{	for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				A[i][j] = (float) i + j;
		MPI_Send(A, 1, index_mpi_t, 1, 0, MPI_COMM_WORLD);
		// starts sending from &A[0][0]
	}
	else
	{	/* my_rank == 1 */
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				T[i][j] = 0.0;
		// we could have stored all the entries in a linear array as well!! type signature remains the same
		MPI_Recv(T, 1, index_mpi_t, 0, 0, MPI_COMM_WORLD, &status);
		// Note that data type defines how the elements are stores. Refer to send_triangle_modified.c
		for (i = 0; i < n; i++)
		{	for (j = 0; j < n; j++)
				printf("%4.1f ", T[i][j]);
			printf("\n");
		}
	}
	MPI_Finalize();
}
