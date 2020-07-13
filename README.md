# DSGD
Distributed Stochastic Gradient Descent (DSGD) algorithm for matrix factorization, paralleled by MPI.

## 1 Description

### 1.1 Background

Matrix factorization is to decompose the sparse matrix R into two matrix U and V, and enables R≈UV. SGD is an algorithm to solve the matrix factorization problem, and DSGD is a parallized SGD algorithm proposed in [1]. EsPa is a partition method originally used in DSGD, which may cause load imbalance and lowering the parallel efficiency. BaPa is an advanced partition method which effectively alleviates load imbalance problem and usually makes the program run faster than EsPa. To get more information about EsPa and BaPa, see our paper [2]. 

### 1.2 MPI implementation

Data transmission is implemented by blocking sending (MPI_Send) and non-blocking receiving (MPI_Irecv and MPI_Wait), which can effectively reduce the communication cost.

### 1.3 How to use our program

Before running our program, you need to prepare a dataset, whose data need to be arranged in specific format (see Section 2.1). Then you need to compile the program. Some marco values are required to be specified by adding them to the CFLAG argument, or you will receive compilation error (see Section 2.2). To run the program, you need to specify the number of MPI tasks before submit it to the cluster (see Section 2.3).

The program will generate 3 files: "blockfile", "rtag.txt" and "record.txt". "blockfile" records the partitioning result, which is, the nonzeros in each 2D block. "rtag.txt" records the number of nonzeros in each 2D block. "record.txt" records the RMSE result of each iteration and performance result. When iterating finishes, the statics of "loop count" and "average elapsed time per iteration" as the performance result are appended to "record.txt".

The program currently does not output anything more than the 3 files above. If you want to see more details, such as the training result of U, V and R, you need to write print code yourself.

## 2 Getting Started

### 2.1 Prepare Dataset

The sparse matrix R should be stored in COO format, i.e, each nonzero is stored as a triple of (row index | column index | value). Details of the three elements are as follows:

|NAME			    | TYPE			|    RANGE       |
| --- | :---: | --- |
|row index	  | integer	  |		1 to USERS   |
|column index | integer	  |		1 to ITEMS   |
|value			  | double		| 	>=0          |

The nonzeros should be sorted by row index in ascending order. For those nonzeros having the same row index, they should be sorted by column index in ascending order.

The user ids of original dataset may be incontinuous. For example, they may be (1, 3, 10, 20...) We made a mapping of (1, 3, 10, 20...)->(1, 2, 3, 4...) so that the user ids could be continuous. Such mapping is also used to deal incoutinuous item ids.

We have provided a dataset "movielens1m.dat" for example.

### 2.2 Complie

Severel marcos are required to specify when compiling our program. Here are the lists of these marcos:

|  MACRO NAME        |   MACRO VALUE TYPE  |   DESCRIPTION |
| --- | :---: | --- |
|      ROW           |        integer      | The number of row partitioning. Matrix R is partitioned into ROW * COL blocks, where COL = ROW. |
|      USERS         |        integer      | The number of users. |
|      ITEMS         |        integer      | The number of items. |
|  DATASET_PATH      |        string       | The path of your dataset. |
|ALGO_BAPA ALGO_ESPA |                     | The partition algorithm. Choose either BAPA or ESPA. |

When using "make" to compile the program, these marcos can be specified in the format 

```
CFLAGS+=-D<macro name (=macro value)>
```

Here is a compilation example:

```
make CFLAGS+=-DROW=12 CFLAGS+=-DUSERS=6040 CFLAGS+=-DITEMS=3952 CFLAGS+=-DALGO_BAPA CFLAGS+=-DDATASET_PATH=./movielens1m.dat
```

which can also be written as

```
make CFLAGS+="-DROW=12 -DUSERS=6040 -DITEMS=3952 -DALGO_BAPA -DDATASET_PATH=./movielens1m.dat"
```

### 2.3 Run

The program need an extra task to collect the results from other tasks. The extra task does not paticipate in parallel computation.  Remember that you have already compiled the program with a specific ROW (see Section 3.1). Now, if you want to run the program, you need to submit it with ROW+1 tasks.

The MPI program can be executed by specifying the task number when submitted to the cluster. On Tian-He2 platform, we use "yhrun" command to submit MPI program:

```
yhrun -N <minimum node number> -n <minimum task number> ./DSGD.exe
```

For example, if you compile the program by setting CFLAGS+=-DROW=12, the submitting command becomes:

```
yhrun -N 13 -n 13 ./DSGD.exe
```

It is OK if the "-N" argument less than 13, since a node may have several CPUs, and each CPU may have several cores. The minimum requirement is to ensure the task number not below the core number used, since one task is executed independently by one core.

Empirically, when "minimum node number" equals to "minimum task number", i.e., each node run only one task, the computation time is the lowest. If 2 or more tasks run on one node, they may scramble for computation resources and thus causing longer computation time.

If not on Tian-He2 platform, the command may be different.

## 3 Future Work
Here are some tips to further optimize our program, which have not been fully tested at present. We take them as our future work.

* Store data in lower precision
Our program stores the values in double precision. Actually high precision is not necessary in Machine Learing area. Nowadays many machine learning software incline to store data in single precision or half-single precision, in order to make the program run faster.

* Use optimizing argument 
Try "-O3" argument when compiling the program, e.g., adding "CFLAGS+=-O3" to the compilation example in Section 3.1. The argument "-O3" tells the compiler to produce a more optimized assemble code, which enables the program to run faster but probably with a slight precision loss.


## References:
[1] R. Gemulla, E. Nijkamp, P. J. Haas, and Y. Sismanis, “Large-scale matrix factorization with distributed stochastic gradient descent,” in Proceedings of the 17th ACM SIGKDD international conference on Knowledge discovery and data mining. ACM, 2011, pp. 69–77.

[2] R. Guo et al., "BaPa: A Novel Approach of Improving Load Balance in Parallel Matrix Factorization for Recommender Systems," in IEEE Transactions on Computers, doi: 10.1109/TC.2020.2997051.
