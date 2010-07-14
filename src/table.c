#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "pit.h"
#include "table.h"

#define TABLE_INCREMENT       5
#define TABLE_HAS_ID          1
#define TABLE_HAS_CREATED_AT  2
#define TABLE_HAS_UPDATED_AT  4
#define TABLE_HAS_TIMESTAMPS  (TABLE_HAS_CREATED_AT | TABLE_HAS_UPDATED_AT)

/*
** Initialize the table by alloocating necessary memory chunks.
*/
PTable table_initialize(ulong record_size, ulong flags) {
    PTable pt;

    pt = calloc(1, sizeof(Table));
    pt->flags             = flags | TABLE_HAS_ID;
    pt->record_size       = record_size;
    pt->number_of_slots   = TABLE_INCREMENT;
    pt->number_of_records = 0;
    pt->auto_increment    = 0;
    pt->slots             = calloc(TABLE_INCREMENT, pt->record_size);
    pt->index             = calloc(TABLE_INCREMENT, sizeof(uchar *));

    return pt;
}

/*
** Return the address of next avaiable slot within pt->slots chunk.
*/
static uchar *table_available_slot(PTable pt) {
    return pt->slots + pt->number_of_records * pt->record_size;
}

/*
** Return the address of last stored record.
*/
static uchar *table_last_record(PTable pt) {
    if (pt->number_of_records == 0) {
        return pt->slots;
    } else {
        return table_available_slot(pt) - pt->record_size;
    }
}

/*
** Return the address of next available pointer within pt->index chunk.
*/
static uchar **table_available_index(PTable pt) {
    return pt->index + pt->auto_increment;
}

/*
** Return the address of last pointer within pt->index chunk.
*/
static uchar **table_last_index(PTable pt) {
    if (pt->auto_increment == 0) {
        return pt->index;
    } else {
        return table_last_index(pt) - sizeof(char *);
    }
}

/*
** Extend the table involves three steps. First, we reallocate pt->slots
** chunk adding TABLE_INCREMENT empty slots. Then we add the same number
** of empty indices to the the pt->index chunk. Finally, we adjust
** existing indices to make them point to reallocated record slots.
*/
static PTable table_extend(PTable pt) {
    register ulong i;
    register uchar **pi;

    pt->number_of_slots += TABLE_INCREMENT;
    /*
    ** Re-allocate the records and set newly added memory chunk to 0.
    */
    pt->slots = realloc(pt->slots, pt->number_of_slots * pt->record_size);
    memset(table_available_slot(pt), 0, TABLE_INCREMENT * pt->record_size);
    /*
    ** Re-allocate the index and set newly added memory chunk to 0.
    */
    pt->index = realloc(pt->index, pt->number_of_slots * sizeof(char *));
    memset(table_available_index(pt), 0, TABLE_INCREMENT * sizeof(char *));
    /*
    ** Reassign index entries to point to the re-allocatted records.
    */
    for (i = 0, pi = pt->index;  i < pt->auto_increment;  i++, pi++) {
        if (*pi != NULL) {
            *pi = pt->slots + i * pt->record_size;
        }
    }
    return pt;
}

/*
** Find a record by id and return its address.
*/
uchar *table_find(PTable pt, ulong id) {
    if (pt->number_of_records == 0 || id <= 0 || id > pt->auto_increment) {
        return NULL;
    } else {
        return *(pt->index + id - 1);
    }
}

/*
** Delete a record by its ID. Return the address of next record or NULL
** if deleted last record.
*/
uchar *table_delete(PTable pt, ulong id) {
    register uchar *pr = (uchar *)table_find(pt, id);

    if (pr) {
        register ulong i;
        register uchar **pi;
        register uchar *last = table_last_record(pt);
        /*
        ** Overwrite current record by shifting over remaining records
        */
        if (pr != last) {
            memmove(pr, pr + pt->record_size, last - pr);
        }
        /*
        ** Set the slot occupied by the last record to zero.
        */
        memset(last, 0, pt->record_size);
        /*
        ** Set current record pointer to NULL, then update the rest of
        ** the index to point to the shifted records.
        */
        pi = pt->index + id - 1;  
       *pi++ = NULL;

        for (i = id;  i < pt->auto_increment; i++) {
            *pi++ -= pt->record_size;
        }
        pt->number_of_records--;
    }

    if (pr && pt->number_of_records > 0) {
        return pr;
    } else {
        return NULL;
    }
}

/*
** Insert a record and return its address. The table gets extended
** as necessary.
*/
uchar *table_insert(PTable pt, uchar *record) {
    register uchar **pi;
    register time_t now;

    if (pt->number_of_records >= pt->number_of_slots) {
        pt = table_extend(pt);
    }

     pi = table_available_index(pt);
    *pi = table_available_slot(pt);
    memmove(table_available_slot(pt), record, pt->record_size);

    pt->number_of_records++;
    pt->auto_increment++;
    /*
    ** Update record id if the table has primary key. The id must be the first
    ** record field of type "unsigned long".
    */
    if (pt->flags & TABLE_HAS_ID) {
        *(ulong *)*pi = pt->auto_increment;
    }
    /*
    ** Update created_at and/or updated_at which must be last one or two record
    ** fields of type "time_t".
    */
    if (pt->flags & TABLE_HAS_CREATED_AT || pt->flags & TABLE_HAS_UPDATED_AT) {
        now = time(NULL);
        *(time_t *)(*pi + pt->record_size - sizeof(time_t)) = now;
    }
    if (pt->flags & TABLE_HAS_CREATED_AT && pt->flags & TABLE_HAS_UPDATED_AT) {
        *(time_t *)(*pi + pt->record_size - sizeof(time_t) * 2) = now;
    }
    return *pi;
}

/*
** Release pt->slots and pt->index memory chunks, then free the table itself.
*/
PTable table_free(PTable pt) {
    if (pt) {
        if (pt->index) {
            free(pt->index);
        }
        if (pt->slots) {
            free(pt->slots);
        }
        free(pt);
    }

    return NULL;
}

/*
** Save the contents of the table to file. The file handle should be
** open with for writing.
*/
int table_save(FILE *file, PTable pt) {
    register int written = 0;
    /*
    ** Save table header data: flags, record_size, number_of_slots,
    ** number_of_records, and auto_increment.
    */
    written += fwrite(pt, sizeof(ulong), 5, file);
    /*
    ** Save the records. Note that we save the actual (not allocated) data.
    */
    written += fwrite(pt->slots, pt->record_size, pt->number_of_records, file);
    return written;
}

/*
** Load the contents of the table from file. The file handle should be
** open with for reading.
*/
PTable table_load(FILE *file) {
    PTable pt;
    register int read = 0;
    register ulong i;
    uchar *pr, **pi;
    
    pt = calloc(1, sizeof(Table));
    /*
    ** First read the header.
    */
    read += fread(pt, sizeof(ulong), 5, file);
    /*
    ** Now allocate slots and index based n original  number of slots.
    */
    printf("Allocating %lu slots\n", pt->number_of_slots);
    pt->slots = pr = calloc(pt->number_of_slots, pt->record_size);
    pt->index = pi = calloc(pt->number_of_slots, sizeof(uchar *));
    /*
    ** Now read the records into the slots and rebuild the index.
    */
    read += fread(pt->slots, pt->record_size, pt->number_of_records, file);
    for(i = 0;  i < pt->number_of_records;  i++, pi++) {
        if ((ulong)*pr == i + 1) {
            *pi = pr;
            pr += pt->record_size;
        }
    }
    return pt;
}


/* #define TEST */
#if defined(TEST)

typedef struct {
    ulong id;
    ulong value;
    char name[30];
    time_t created_at;
    time_t updated_at;
} Record;

void dump(Record *prec) {
    if (prec) {
        printf("(%08lX) id: %08lu, value: %lu, created_at: %lu, updated_at: %lu)\n", 
        (ulong)prec, prec->id, prec->value, prec->created_at, prec->updated_at);
    } else {
        printf("(NULL)\n");
    }
}

void dump_all(PTable pt) {
    int i;
    Record *prec;

    for(i = 1;  i <= pt->number_of_slots;  i++) {
        prec = (Record *)table_find(pt, i);
        if (prec) {
            dump(prec);
        } else {
            printf("%d not found\n", i);
        }
    }
}

int main() {
    PTable pt;

    Record rec, *prec;
    ulong i, total = 30;

    pt = table_initialize(sizeof(Record), TABLE_HAS_ID | TABLE_HAS_TIMESTAMPS);
    for(i = 0;  i < total;  i++) {
        rec.id = 0;
        rec.value = 0x11223344 + i + 1;
        strcpy(rec.name, "test");
        rec.created_at = rec.updated_at = (time_t)0;

        prec = (Record *)table_insert(pt, (uchar *)&rec);
        dump(prec);
    }
    for (i = 20;  i < total;  i++) {
        printf("Deleting %lu\n", i + 1);
        prec = (Record *)table_delete(pt, i + 1);
    }
    dump_all(pt);

    FILE *file = fopen("/tmp/.pit", "w");
    table_save(file, pt);
    fclose(file);

    file = fopen("/tmp/.pit", "r");
    pt = table_load(file);
    dump_all(pt);
    fclose(file);

    table_free(pt);
    printf("OK\n");
    return 0;
}
#endif
