#include "index.h"
#include "pes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ─────────────────────────────────────────────
// PROVIDED: find entry
IndexEntry *index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

// ─────────────────────────────────────────────
// PROVIDED: remove entry
int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            for (int j = i; j < index->count - 1; j++) {
                index->entries[j] = index->entries[j + 1];
            }
            index->count--;
            return 0;
        }
    }
    return -1;
}

// ─────────────────────────────────────────────
// PROVIDED: status
int index_status(const Index *index) {
    printf("Staged changes:\n");

    if (index->count == 0) {
        printf("  (nothing to show)\n");
    } else {
        for (int i = 0; i < index->count; i++) {
            printf("  staged: %s\n", index->entries[i].path);
        }
    }

    printf("\nUnstaged changes:\n");
    printf("  (nothing to show)\n");

    printf("\nUntracked files:\n");
    printf("  (nothing to show)\n");

    return 0;
}

// ─────────────────────────────────────────────
// comparator
static int cmp_entries(const void *a, const void *b) {
    const IndexEntry *ea = a;
    const IndexEntry *eb = b;
    return strcmp(ea->path, eb->path);
}

// ─────────────────────────────────────────────
// LOAD
int index_load(Index *index) {
    if (!index) return -1;

    index->count = 0;

    FILE *f = fopen(".pes/index", "r");
    if (!f) return 0;

    while (1) {
        IndexEntry e;
        char hex[65];

        int ret = fscanf(f, "%o %64s %u %[^\n]\n",
                         &e.mode, hex, &e.size, e.path);

        if (ret != 4) break;

        hex_to_hash(hex, &e.hash);

        if (index->count >= MAX_INDEX_ENTRIES) break;

        index->entries[index->count++] = e;
    }

    fclose(f);
    return 0;
}

// ─────────────────────────────────────────────
// SAVE (FIXED — no stack overflow)
int index_save(const Index *index) {
    if (!index) return -1;

    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;

    int count = index->count;
    if (count < 0) count = 0;
    if (count > MAX_INDEX_ENTRIES) count = MAX_INDEX_ENTRIES;

    // 🔥 dynamic allocation (fixes crash)
    IndexEntry *temp = malloc(sizeof(IndexEntry) * count);
    if (!temp) {
        fclose(f);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        temp[i] = index->entries[i];
    }

    qsort(temp, count, sizeof(IndexEntry), cmp_entries);

    for (int i = 0; i < count; i++) {
        char hex[65];
        hash_to_hex(&temp[i].hash, hex);

        fprintf(f, "%o %s %u %s\n",
                temp[i].mode, hex, temp[i].size, temp[i].path);
    }

    free(temp);

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    rename(".pes/index.tmp", ".pes/index");
    return 0;
}

// ─────────────────────────────────────────────
// ADD
int index_add(Index *index, const char *path) {
    if (!index || !path) return -1;

    index_load(index);

    struct stat st;
    if (stat(path, &st) < 0) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    size_t size = st.st_size;

    char *data = malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }

    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return -1;
    }

    fclose(f);

    ObjectID id;
    if (object_write(OBJ_BLOB, data, size, &id) < 0) {
        free(data);
        return -1;
    }

    IndexEntry *e = index_find(index, path);

    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) {
            free(data);
            return -1;
        }
        e = &index->entries[index->count++];
    }

    e->mode = 100644;
    e->size = size;
    strcpy(e->path, path);
    e->hash = id;

    free(data);

    return index_save(index);
}

// phase 3 improvement
// phase 3 improvement
// minor improvement
