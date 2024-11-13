# In-kernel Cuckoo Hashing
 
## Execution Flow

- To compile the model you need to type on the Linux command line, in the correct folder, the following instructions:

   \- `make` will generate different files.

   \- `sudo` insmod cuckoo_hash_kernel.ko' will start the kernel.

  \- `sudo rmmod cuckoo_hash_kernel` will terminate the kernel execution.

- These are the supported operations:

   \- Determine whether a key is stored (if so, output its value) or not.

   `echo "key" | sudo tee /sys/kernel/cuckoo_hash/get>/dev/null`

   \- Insert a key with its relative value.

   `echo "key=value" | sudo tee /sys/kernel/cuckoo_hash/insert>/dev/null`

   \- Delete a key.

   `echo "key" | sudo tee /sys/kernel/cuckoo_hash/delete>/dev/null`

   \- Print the set of keys with their relative values.

   `sudo cat /sys/kernel/cuckoo_hash/print`

- To visualize the output printed by the module you have to use the command `sudo dmesg`.

## Test

-The file `test.c` tests the execution of the algorithm simulating an execution with a defined number of commands, terminals (that execute in parallel different commands) and words.
To execute type on the command line: `python3 test.py NUM_COMMANDS NUM_TERMINALS NUM_WORLDS`.

 \- `NUM_COMMANDS` indicates the number of commands that will be executed in each terminal.
 
 \- `NUM_TERMINALS` is the number of terminals that will execute the different commands in parallel.
 
 \- `NUM_WORLDS` indicates the number of words that will be present in the vocabulary.

 The program will print the commands that are executed with their associated terminal number that execute it, the responses given by the kernel, the final values of the words in the vocabulary, and at the end it will print `The result is possible` or `The result is not possible`. The result is possible if the value associated with a given word in the last execution of one of the different terminals. It should always return `The result is possible`.
