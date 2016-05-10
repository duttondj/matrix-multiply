# matrix-multiply
Multiply matrices using pthreads. Run `./matrix-multiply <matrix A> <matrix B> <output file>` The input matrices should be made up of integers where each rown is on its own line and columns are seperated with whitespace. Examples can be found in `testcases/`.

This program uses a multithreaded approach to multiply two matrices together. A seperate thread is used for each element of the product. A mutex is used to control when a temp.txt file is available for writing to for determining the final result. A better approach would have been to use pthread exit calls as a method to pass the product matrix elements back to the main thread but that would avoid implementing sychronization via a mutex.

## Compile
Run `qmake` then `make` or just run `g++ -lpthread -o matrix-multiply main.cpp`
