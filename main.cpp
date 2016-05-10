/* 	Danny Dutton
	duttondj@vt.edu
	905570580
	
	ECE 3574
	Project 6: Multithreading and Synchronization

	This program uses a multithreaded approach to multiply two matrices
	together. A seperate thread is used for each element of the product. A 
	mutex is used to control when a temp.txt file is available for writing to 
	for determining the final result. 

	The mutex is used instead of a semaphore due to the exclusive nature of a 
	mutex. There is only one file to write to so I don't want a multi-access
	semaphore and having a single use semaphore is redundent. I used one in the
	thread function to prevent access to the temp.txt file when a thread is 
	trying to write its answer to it. Each thread must write to a common file 
	and all threads could possibly be running at the same time so some sort
	of synchronization is needed to control access to it. I also use the
	mutex to access temp.txt when the main function reads it to create the
	final output file. This may not be totally necessary but it ensures that
	there is no one else using it or waiting for access to it. All threads 
	should have already been exited and joined back to the main thread by that
	point in the code.

	A better approach would have been to use pthread exit calls as a method to 
	pass the result matrix elements back to the main thread but that would 
	avoid implementing sychronization.
*/

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>


// Struct defining a matrix, number of rows/cols
// and a vector containing elements in order.
struct Matrix {
	int rows;
	int cols;
	std::vector<int> element;
};

// Struct containing data to be passed to a thread.
// This is the row of A and col of B it uses to calculate
// the cooresponding C matrix element.
struct thread_data {
	int row;
	int col;
};

// Operands matrices
struct Matrix A, B;

// Mutex for synchronization
pthread_mutex_t mutex;

// Multithread function, 
void *MatrixMult(void *rowcol)
{
	int row;
	int col;

	// Get row and col number from parameter
	struct thread_data *my_rowcol;
	my_rowcol = (struct thread_data *) rowcol;
	row = my_rowcol->row;
	col = my_rowcol->col;

	// Determine C(row, col) element value
	int c = 0;
	for (int i = 0; i < A.cols; i++)
	{
		c += A.element[i+(row*A.cols)] * B.element[col+(i*B.cols)];
	}

	// Create string containing element position and value
	std::ostringstream ss;
	ss << "C" << row << "," << col << "=" << c << std::endl;

	// wait for mutex to be available
	// check if any issues and exit if so
	int rc = pthread_mutex_lock(&mutex);
	if (rc)
	{
          printf("ERROR: pthread_mutex_lock: %d at element %d %d\n", rc, row, col);
          exit(-1);
    }

	// Write product elements to temp.txt file
    std::ofstream fout("temp.txt", std::ios_base::app);
    
    if (!fout)
    {
    	std::cerr << "ERROR: Could not open file: temp.txt" << std::endl;
		exit(-1);
    }

    fout << ss.str();
	fout.close();

	// give up mutex
	// check if any issues and exit if so
	rc = pthread_mutex_unlock(&mutex);
	if (rc)
	{
          printf("ERROR: pthread_mutex_unlock: %d at element %d %d\n", rc, row, col);
          exit(-1);
    }

    // Kill the thread
    pthread_exit(NULL);
}

void parseMatrixFile(std::string filename, struct Matrix * M)
{
	int e;				// Element to be parsed
	std::string line;	// Line from text file
	int tempcols = 0;	// Current number of columns in line

	// Initialize rows to zero
	// Columns to -1 since we want to ensure that its untouched
	// since there can never be -1 items in a line
	M->rows = 0;
	M->cols = -1;

	// Open filename
	std::ifstream fin(filename.c_str());

	// Check if its good
	if (!fin)
	{
		std::cerr << "ERROR: Could not open file: " << filename << std::endl;
		exit(-1);
	}

	// Read the file
	while (fin.good())
	{
		// Read each line
		while(getline(fin, line))
		{
			// Get each element and add it to the matrix
			std::istringstream iss(line);
			while (iss >> e)
			{
				M->element.push_back(e);
				tempcols++;
			}

			// First row so record number of columns
			if (M->cols == -1)
				M->cols = tempcols;

			// At least second row but current columns
			// don't match previous
			else if (M->cols != tempcols)
			{
				std::cerr << "ERROR: Incomplete matrix or incorrect format " << filename << std::endl;
				exit(-1);
			}		
			// Reset current col count and increment row count
			tempcols = 0;
			M->rows++;
		}
	}

	// Done with the text file, close it
	fin.close();
}

// Main, reads operand matrices, creates threads, creates result file
int main(int argc, char ** argv)
{
	// Check args
	if (argc != 4)
	{
		std::cerr << "ERROR: Invalid number of arguments" << std::endl;
		std::cerr << "Usage: " << argv[0] << " input-file1 input-file2 outfile" << std::endl;
		exit(-1);
	}

	// Parse the input matrices and save to Matrix structs
	parseMatrixFile(argv[1], &A);
	parseMatrixFile(argv[2], &B);

	// Check if A and B can be multiplied
	if (A.cols != B.rows)
	{
		std::cerr << "ERROR: Cannot multiply, dimmension mismatch" << std::endl;
		return 1;
	}

	// Create the array of thread_data
	std::vector<struct thread_data> rowcol;
	struct thread_data temp;
	for (int i = 0; i < A.rows; i++)
	{
		for (int j = 0; j < B.cols; j++)
		{
			temp.row = i;
			temp.col = j;
			rowcol.push_back(temp);
		}
	}

	// Clear out contents of temp.txt before the threads try to append to it
	std::ofstream tempfile;
	tempfile.open("temp.txt", std::ofstream::out | std::ofstream::trunc);
	tempfile.close();

	// Setup mutex with default params
	pthread_mutex_init(&mutex, NULL);

	// For each in rowcol vector, pass them to their own thread
	pthread_t threads[rowcol.size()];
	pthread_attr_t attr;
	int rc;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	for (unsigned int i = 0; i < rowcol.size(); i++)
	{
		rc = pthread_create(&threads[i], &attr, MatrixMult, (void *) &rowcol[i]);
	  	if (rc) {
	    	printf("ERROR: pthread_create(): %d\n", rc);
	    	exit(-1);
	    }
	}

	// Free attribute and wait for the other threads
	pthread_attr_destroy(&attr);
	for(unsigned int i = 0; i < rowcol.size(); i++)
	{
		rc = pthread_join(threads[i], NULL);
		if (rc)
		{
			printf("ERROR return code from pthread_join() is %d\n", rc);
			exit(-1);
		}
	}

	// take/wait for mutex to be available
	// check if any issues and exit if so
	rc = pthread_mutex_lock(&mutex);
	if (rc)
	{
          printf("ERROR: pthread_mutex_lock: %d\n", rc);
          exit(-1);
    }

    // Create vector of ints for the result
	std::vector<int> result(rowcol.size());

	// Open temp.txt
	std::ifstream fin("temp.txt");

	// Check if its good
	if (!fin)
	{
		std::cerr << "ERROR: Could not open file: temp.txt" << std::endl;
		exit(-1);
	}

	// Read the file
	while (fin.good())
	{
		std::string line;

		// Read each line
		while(getline(fin, line))
		{
			// Remove the C, replace comma with space, replace = with space
			// from: Cr,c=x
			// to: 	 r c x
			// temp.txt still is written to according to spec but
			// tight formmating and unnecessary chars are replaced
			// with whitespace to make parsing easier
			line.erase(0,1);
			std::replace(line.begin(), line.end(), ',', ' ');
			std::replace(line.begin(), line.end(), '=', ' ');

			std::istringstream iss(line);
			int r, c, value;

			// Get the row, col, ignore the =, then get the value
			iss >> r >> c >> value;

			// Store it in the result array at the cooresponding element
			result[(B.cols * r) + c] = value;
		}
	}

	// Done with the text file, close it
	fin.close();

	// give up mutex
	// check if any issues and exit if so
	rc = pthread_mutex_unlock(&mutex);
	if (rc)
	{
          printf("ERROR: pthread_mutex_unlock: %d\n", rc);
          exit(-1);
    }

	// Write product elements to output file
    std::ofstream fout(argv[3]);
    
    // Make sure its good to write to
    if (!fout)
    {
    	std::cerr << "ERROR: Could not open file: " << argv[3] << std::endl;
		exit(-1);
    }

    // Write each element, line break on new rows
    for (unsigned int i = 0; i < result.size(); i++)
    {
    	fout << result[i] << "\t";

	 	if ((i+1) % B.cols == 0)
	 		fout << std::endl;
    }

	fout.close();

	// Kill the thread
    pthread_exit(NULL);
}