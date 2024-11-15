#include "cuckoo_hash_kernel.h"
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

static DEFINE_MUTEX(mutex);

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

int ckh_insert(CKHash_Table *D, const unsigned char *key, int value, int count)
{
	if(count == 0) mutex_lock(&mutex);
	unsigned long h1, h2;
	unsigned int j;
	CKHash_Cell x, temp;
	
	/*
	 * If the element is already in D, then overwrite and return.
	 */
	ckh_hash(&h1, D->a1, D->function_size, D->table_size, D->shift, key);

	if (D->T1[h1].key != NULL && KEY_CMP(D->T1[h1].key, key) == 0) {
		D->T1[h1].value = value;
		mutex_unlock(&mutex);
		return FALSE;
	}

	ckh_hash(&h2, D->a2, D->function_size, D->table_size, D->shift, key);
	if (D->T2[h2].key != NULL && KEY_CMP(D->T2[h2].key, key) == 0) {
		D->T2[h2].value = value;
		mutex_unlock(&mutex);
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
			mutex_unlock(&mutex);
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
			mutex_unlock(&mutex);
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

	ckh_insert(D, x.key, x.value, 1);
	kfree(x.key); /* Free x.key, because it is already copied. */

	mutex_unlock(&mutex);
	return TRUE;
}

int ckh_get(CKHash_Table *D, const unsigned char *key, int *ret_value)
{
	mutex_lock(&mutex);
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

	mutex_unlock(&mutex);
	return found;
}

int ckh_delete(CKHash_Table *D, const unsigned char *key)
{
	mutex_lock(&mutex);
	unsigned long hkey;

	ckh_hash(&hkey, D->a1, D->function_size, D->table_size, D->shift, key);
	if (D->T1[hkey].key != NULL) {
		if (KEY_CMP(D->T1[hkey].key, key) == 0) {
			kfree(D->T1[hkey].key);
			D->T1[hkey].key = NULL;
			D->size--;
			if (D->size < D->min_size)
				ckh_rehash(D, D->table_size / 2);
			mutex_unlock(&mutex);
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
			mutex_unlock(&mutex);
			return TRUE;
		}
	}

	mutex_unlock(&mutex);
	return FALSE;
}

void ckh_print(CKHash_Table *D)
{
	mutex_lock(&mutex);
	unsigned int i;

	for (i = 0; i < D->table_size; i++) {
		if (D->T1[i].key != NULL)
			pr_info("%s\t%d\n", D->T1[i].key, D->T1[i].value);
		if (D->T2[i].key != NULL)
			pr_info("%s\t%d\n", D->T2[i].key, D->T2[i].value);
	}

	mutex_unlock(&mutex);
}

//////////////////////////////////////////////////////////////////////////////////////////

static ssize_t get_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return snprintf(buf, PAGE_SIZE, "%s\n", "");
}

static ssize_t get_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    if (count >= MAX_BUF_LEN)
        return -EINVAL;

	static char word_to_get[MAX_BUF_LEN] = "";
    strncpy(word_to_get, buf, count - 1);
	
	int ret_value = 0;
	if (ckh_get(D, (unsigned char *)word_to_get, &ret_value))
		pr_info("found [%s] value=%d\n", word_to_get, ret_value);
	else
		pr_info("could not find [%s]\n", word_to_get);

    return count;
}

static ssize_t insert_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "%s\n", "");
}

static ssize_t insert_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    if (count >= MAX_BUF_LEN)
        return -EINVAL;

	static char input_to_insert[MAX_BUF_LEN] = "";
    strncpy(input_to_insert, buf, count - 1);	
	
	int value = 0, dec = 1;
	int length = count;
	int pos = count - 2;
	while(input_to_insert[pos] != '=' || pos == -1){
		value += dec * (input_to_insert[pos] - '0');
		pos--;
		dec *= 10;
	}
	static char word[MAX_BUF_LEN] = "";
	strncpy(word, input_to_insert, pos);
	ckh_insert(D, (unsigned char *)word, value, 0);
    return count;
}

static ssize_t delete_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "%s\n", "");
}

static ssize_t delete_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    if (count >= MAX_BUF_LEN)
        return -EINVAL;

	char word_to_delete[MAX_BUF_LEN] = "";
    strncpy(word_to_delete, buf, count - 1);
	ckh_delete(D, (unsigned char *)word_to_delete);	

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
static struct kobj_attribute delete_attribute = __ATTR(delete, 0660, delete_show, delete_store);
static struct kobj_attribute print_attribute = __ATTR(print, 0660, print_show, print_store);

static struct attribute *attrs[] = {
    &get_attribute.attr,
    &insert_attribute.attr,
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
