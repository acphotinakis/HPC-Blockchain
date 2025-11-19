

### Step 1: Setup Project (main.cpp)

- Setup a logger class object to log  messages throughout execution
- pass the argument count and arg values to MPI_Init to create the environment
- Pass MPI_COMM_WORLD to MPI_Comm_rank and MPI_Size_rank
- Using config class to parse out the arguments for running the simulation, which include
    1.  number of node
    2.  number of shards
    3.  number of transactions
    4.  whether or not the logger is verbose
- Then generate a set of mock transactions
- Once mock transactions are generated, create each shard
- Each shared 1 to N 
    - Contains 3 validator nodes
    - Contains 1 leader node
- create a blockchain object
- loop through each transaction and process them for the shards
- 