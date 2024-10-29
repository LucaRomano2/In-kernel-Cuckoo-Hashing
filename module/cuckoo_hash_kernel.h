#ifndef CUCKOO_HASH_KERNEL_H
#define CUCKOO_HASH_KERNEL_H

#define MIN_TAB_SIZE (4)
#define MAX_LINE_LEN (8192)
#define MAX_BUF_LEN 20

#define TRUE 1
#define FALSE !TRUE
#define MAX_FUNCTION_SIZE 128
#define CKH_MAX_DECIMAL_FORMAT 64
#define MALLOC(type, n) (type *)kmalloc((n) * sizeof(type), GFP_KERNEL)
#define REALLOC(data, type, n) (type *)realloc(data, n * sizeof(type))
#define KEY_CMP(s1, s2) strcmp((const char *)s1, (const char *)s2)

typedef struct {			/* Hash table cell type */
	unsigned char *key;		/* Hash key */
	int value;			/* Hash value */
} CKHash_Cell;

typedef struct {
	unsigned int size;		/*  Current size */
	unsigned int table_size;	/*  Size of hash tables */
	int shift;			/*  Value used for hash function */
	unsigned int min_size;		/*  Rehash trigger size */
	unsigned int mean_size;		/*  Rehash trigger size */
	unsigned int max_chain;		/*  Max iterations in insertion */
	CKHash_Cell *T1;		/*  Pointer to hash table 1 */
	CKHash_Cell *T2;		/*  Pointer to hash table 2 */
	int function_size;		/*  Size of hash function */
	int *a1;			/*  Hash function 1 */
	int *a2;			/*  Hash function 2 */
} CKHash_Table;

CKHash_Table *D;

CKHash_Table *ckh_construct_table(int min_size);

int ckh_insert(CKHash_Table *D, const unsigned char *key, int value, int count);
int ckh_get(CKHash_Table *D, const unsigned char *key, int *ret_value);
int ckh_delete(CKHash_Table *D, const unsigned char *key);
void ckh_print(CKHash_Table *D);
void free_word(char* word);

#endif /* CUCKOO_HASH_KERNEL_H */
