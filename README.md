# In-kernel lock-less Cuckoo Hashing
 
## Execution Flow

- To compile the model you need to type on the Linux command line, in the correct folder, the following instructions:

   \- `make` will generate differents files.

   \- `sudo` insmod cuckoo_hash_kernel.ko' will start the kernel.

  \- `sudo rmmod cuckoo_hash_kernel` will terminate the kernel execution.

- These are the supported operations:

   \- Determine whether a key is stored (if so, output its value) or not.

   `echo "TT" | sudo tee /sys/kernel/cuckoo_hash/get>/dev/null`

   \- Insert a key with its relative value.

   `echo "key" | sudo tee /sys/kernel/cuckoo_hash/insert>/dev/null`

   `echo value | sudo tee /sys/kernel/cuckoo_hash/value>/dev/null`

   \- Delete a key.

   `echo "key" | sudo tee /sys/kernel/cuckoo_hash/delete>/dev/null`

   \- Print the set of keys with their relative values.

   `sudo cat /sys/kernel/cuckoo_hash/print`

- To visualize the output printed by the module you have to use the command `sudo dmesg`.
