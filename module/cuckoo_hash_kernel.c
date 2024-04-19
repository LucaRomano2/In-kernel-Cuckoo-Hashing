#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/math.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/kthread.h>

#define MIN_TAB_SIZE (4)
#define MAX_LINE_LEN (8192)
#define MAX_BUF_LEN 20

//////////////////////////////////////////////////////////////////////////////////////////

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

void ckh_init(int a[], int function_size)
{
	int i;
	int random = 0;
	for (i = 0; i < function_size; i++){
    int random_number;
		// Generate a random number using get_random_bytes()
		get_random_bytes(&random_number, sizeof(random_number));
		a[i] = random_number;
	}
}

void ckh_hash(unsigned long *h, int a[], int function_size,
		     int table_size, int shift, const unsigned char *key)
{
	int i;

	*h = 0;
	for (i = 0; key[i]; i++)
		*h ^= (unsigned int)(a[(i % function_size)] * key[i]);
	*h = ((unsigned int)*h >> shift) % table_size;
}

CKHash_Table *ckh_alloc_table(int table_size)
{
	CKHash_Table *D = MALLOC(CKHash_Table, 1);
	D->size = 0;
	D->table_size = table_size;
	D->mean_size = 5 * (2 * table_size) / 12;
	D->min_size = (2 * table_size) / 5;
	D->shift = 30;
	D->max_chain = 12;
	D->T1 = MALLOC(CKHash_Cell, D->table_size);
	D->T2 = MALLOC(CKHash_Cell, D->table_size);
	D->function_size = MAX_FUNCTION_SIZE;
	D->a1 = MALLOC(int, D->function_size);
	D->a2 = MALLOC(int, D->function_size);
	ckh_init(D->a1, D->function_size);
	ckh_init(D->a2, D->function_size);

	return D;
}

CKHash_Table *ckh_construct_table(int min_size)
{
	return ckh_alloc_table(min_size);
}

int ckh_rehash_insert(CKHash_Table *D, unsigned char *key, int value)
{
	unsigned long hkey;
	unsigned int j;
	CKHash_Cell x, temp;

	x.key = key;
	x.value = value;

	for (j = 0; j < D->max_chain; j++) {
		ckh_hash(&hkey, D->a1, D->function_size, D->table_size,
			 D->shift, x.key);
		temp = D->T1[hkey];
		D->T1[hkey] = x;
		if (temp.key == NULL)
			return TRUE;

		x = temp;
		ckh_hash(&hkey, D->a2, D->function_size, D->table_size,
			 D->shift, x.key);
		temp = D->T2[hkey];
		D->T2[hkey] = x;
		if (temp.key == NULL)
			return TRUE;

		x = temp;
	}

	for (j = 0; j < D->table_size; j++) {
		D->T1[j].key = D->T2[j].key = NULL;
		D->T1[j].value = D->T2[j].value = 0;
	}

	ckh_init(D->a1, D->function_size);
	ckh_init(D->a2, D->function_size);

	return FALSE;
}

void ckh_rehash(CKHash_Table *D, int new_size)
{
	CKHash_Table *D_new;
	unsigned int k;

	D_new = ckh_alloc_table(new_size);

	for (k = 0; k < D->table_size; k++) {
		if ((D->T1[k].key != NULL) &&
		    (!ckh_rehash_insert(D_new, D->T1[k].key, D->T1[k].value))) {
			k = -1;
			continue;
		}
		if ((D->T2[k].key != NULL) &&
		    (!ckh_rehash_insert(D_new, D->T2[k].key, D->T2[k].value)))
			k = -1;
	}

	kfree(D->T1);
	kfree(D->T2);
	kfree(D->a1);
	kfree(D->a2);

	D_new->size = D->size;
	*D = *D_new;
	kfree(D_new);
}

int ckh_insert(CKHash_Table *D, const unsigned char *key, int value)
{
	unsigned long h1, h2;
	unsigned int j;
	CKHash_Cell x, temp;
	
	/*
	 * If the element is already in D, then overwrite and return.
	 */
	ckh_hash(&h1, D->a1, D->function_size, D->table_size, D->shift, key);

	if (D->T1[h1].key != NULL && KEY_CMP(D->T1[h1].key, key) == 0) {
		D->T1[h1].value = value;
		return FALSE;
	}

	ckh_hash(&h2, D->a2, D->function_size, D->table_size, D->shift, key);
	if (D->T2[h2].key != NULL && KEY_CMP(D->T2[h2].key, key) == 0) {
		D->T2[h2].value = value;
		return FALSE;
	}
	
	/*
	 * If not, the insert the new element in D.
	 */
	int key_len = strlen((const char *)key) + 1;
	x.key = MALLOC(unsigned char, key_len);

	strncpy((char *)x.key, (const char *)key, key_len);
	x.value = value;

	for (j = 0; j < D->max_chain; j++) {
		temp = D->T1[h1];
		D->T1[h1] = x;
		if (temp.key == NULL) {
			D->size++;
			if (D->table_size < D->size)
				ckh_rehash(D, 2 * D->table_size);
			return TRUE;
		}

		x = temp;
		ckh_hash(&h2, D->a2, D->function_size, D->table_size, D->shift,
			 x.key);
		temp = D->T2[h2];
		D->T2[h2] = x;
		if (temp.key == NULL) {
			D->size++;
			if (D->table_size < D->size)
				ckh_rehash(D, 2 * D->table_size);
			return TRUE;
		}

		x = temp;
		ckh_hash(&h1, D->a1, D->function_size, D->table_size, D->shift,
			 x.key);
	}

	/*
	 * Forced rehash.
	 */
	if (D->size < D->mean_size)
		ckh_rehash(D, D->table_size);
	else
		ckh_rehash(D, 2 * D->table_size);

	ckh_insert(D, x.key, x.value);
	kfree(x.key); /* Free x.key, because it is already copied. */

	return TRUE;
}

int ckh_get(CKHash_Table *D, const unsigned char *key, int *ret_value)
{
	unsigned long hkey;
	int found = FALSE;

	ckh_hash(&hkey, D->a1, D->function_size, D->table_size, D->shift, key);
	if (D->T1[hkey].key != NULL && KEY_CMP(D->T1[hkey].key, key) == 0) {
		*ret_value = D->T1[hkey].value;
		found = TRUE;
	}

	ckh_hash(&hkey, D->a2, D->function_size, D->table_size, D->shift, key);
	if (D->T2[hkey].key != NULL && KEY_CMP(D->T2[hkey].key, key) == 0) {
		*ret_value = D->T2[hkey].value;
		found = TRUE;
	}

	return found;
}

int ckh_delete(CKHash_Table *D, const unsigned char *key)
{
	unsigned long hkey;

	ckh_hash(&hkey, D->a1, D->function_size, D->table_size, D->shift, key);
	if (D->T1[hkey].key != NULL) {
		if (KEY_CMP(D->T1[hkey].key, key) == 0) {
			kfree(D->T1[hkey].key);
			D->T1[hkey].key = NULL;
			D->size--;
			if (D->size < D->min_size)
				ckh_rehash(D, D->table_size / 2);
			return TRUE;
		}
	}

	ckh_hash(&hkey, D->a2, D->function_size, D->table_size, D->shift, key);
	if (D->T2[hkey].key != NULL) {
		if (KEY_CMP(D->T2[hkey].key, key) == 0) {
			kfree(D->T2[hkey].key);
			D->T2[hkey].key = NULL;
			D->size--;
			if (D->size < D->min_size)
				ckh_rehash(D, D->table_size / 2);
			return TRUE;
		}
	}

	return FALSE;
}

void ckh_print(CKHash_Table *D)
{
	unsigned int i;

	for (i = 0; i < D->table_size; i++) {
		if (D->T1[i].key != NULL)
			pr_info("%s\t%d\n", D->T1[i].key, D->T1[i].value);
		if (D->T2[i].key != NULL)
			pr_info("%s\t%d\n", D->T2[i].key, D->T2[i].value);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////

static DEFINE_MUTEX(mutex);
static char word_to_insert[MAX_BUF_LEN] = "";
static char word_to_delete[MAX_BUF_LEN] = "";
static char word_to_get[MAX_BUF_LEN] = "";
static int value_to_insert = 0;

void free_word(char* word){
	for(int i = 0; i < MAX_BUF_LEN; i++){
		word[i] = '\0';
	}
}

static ssize_t get_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return snprintf(buf, PAGE_SIZE, "%s\n", word_to_get);
}

static ssize_t get_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    if (count >= MAX_BUF_LEN)
        return -EINVAL;

	mutex_lock(&mutex);
	free_word(word_to_get);
    strncpy(word_to_get, buf, count - 1);
	
	int ret_value = 0;
	if (ckh_get(D, (unsigned char *)word_to_get, &ret_value))
		pr_info("found [%s] value=%d\n", word_to_get, ret_value);
	else
		pr_info("could not find [%s]\n", word_to_get);
	mutex_unlock(&mutex);
	
    return count;
}

static ssize_t insert_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "%s\n", word_to_insert);
}

static ssize_t insert_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    if (count >= MAX_BUF_LEN)
        return -EINVAL;

	mutex_lock(&mutex);
	free_word(word_to_insert);
    strncpy(word_to_insert, buf, count - 1);	
	
    return count;
}

static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "%d\n", value_to_insert);
}

static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	sscanf(buf, "%d", &value_to_insert);

	ckh_insert(D, (unsigned char *)word_to_insert, value_to_insert);	
	mutex_unlock(&mutex);
	
    return count;
}

static ssize_t delete_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "%s\n", word_to_delete);
}

static ssize_t delete_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    if (count >= MAX_BUF_LEN)
        return -EINVAL;
	mutex_lock(&mutex);
	free_word(word_to_delete);
    strncpy(word_to_delete, buf, count - 1);	
	ckh_delete(D, (unsigned char *)word_to_delete);	
	mutex_unlock(&mutex);

    return count;
}

static ssize_t print_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	ckh_print(D);
    return snprintf(buf, PAGE_SIZE, "");
}

static ssize_t print_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    if (count >= MAX_BUF_LEN)
        return -EINVAL;

    return count;
}

static struct kobj_attribute get_attribute = __ATTR(get, 0660, get_show, get_store);
static struct kobj_attribute insert_attribute = __ATTR(insert, 0660, insert_show, insert_store);
static struct kobj_attribute value_attribute = __ATTR(value, 0660, value_show, value_store);
static struct kobj_attribute delete_attribute = __ATTR(delete, 0660, delete_show, delete_store);
static struct kobj_attribute print_attribute = __ATTR(print, 0660, print_show, print_store);

static struct attribute *attrs[] = {
    &get_attribute.attr,
    &insert_attribute.attr,
    &value_attribute.attr,
    &delete_attribute.attr,
    &print_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static struct kobject *cuckoo_hash_kobj;

static int __init cuckoo_hash_init(void) {
    int retval;

    cuckoo_hash_kobj = kobject_create_and_add("cuckoo_hash", kernel_kobj);
    if (!cuckoo_hash_kobj)
        return -ENOMEM;

    retval = sysfs_create_group(cuckoo_hash_kobj, &attr_group);
    if (retval)
        kobject_put(cuckoo_hash_kobj);

	pr_info("Start");

    D = ckh_construct_table(MIN_TAB_SIZE);

    return retval;
}

static void __exit cuckoo_hash_exit(void) {
    kobject_put(cuckoo_hash_kobj);
    kfree(D);
    pr_info("End");
}

module_init(cuckoo_hash_init);
module_exit(cuckoo_hash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
