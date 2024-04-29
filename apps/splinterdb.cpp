// Copyright 2022 VMware, Inc.
// SPDX-License-Identifier: Apache-2.0
/*
 * SplinterDB "Hello World" example program. Demonstrate use of:
 *  - Basic SplinterDB configuration and create() interface
 *  - Insert, lookup and iterator interfaces
 *  - Close and reopen a SplinterDB db (instance)
 */

#include <cstring>

extern "C" {
#include "splinterdb/default_data_config.h"
#include "splinterdb/splinterdb.h"
}

#define DB_FILE_NAME "splinterdb_intro_db"
#define DB_FILE_SIZE_MB 1024  // Size of SplinterDB device; Fixed when created
#define CACHE_SIZE_MB 32      // Size of cache; can be changed across boots

/* Application declares the limit of key-sizes it intends to use */
#define USER_MAX_KEY_SIZE ((int)100)

/*
 * -------------------------------------------------------------------------------
 * We, intentionally, do not check for errors or show error handling, as this is
 * mostly a sample program to illustrate how-to use the APIs.
 * -------------------------------------------------------------------------------
 */
int main() {
    printf("     **** SplinterDB Basic example program ****\n\n");

    // Initialize data configuration, using default key-comparison handling.
    data_config splinter_data_cfg;
    default_data_config_init(USER_MAX_KEY_SIZE, &splinter_data_cfg);

    platform_set_log_streams(stderr, stderr);

    // Basic configuration of a SplinterDB instance
    splinterdb_config splinterdb_cfg;
    memset(&splinterdb_cfg, 0, sizeof(splinterdb_cfg));
    splinterdb_cfg.filename = DB_FILE_NAME;
    splinterdb_cfg.disk_size = (DB_FILE_SIZE_MB * 1024 * 1024);
    splinterdb_cfg.cache_size = (CACHE_SIZE_MB * 1024 * 1024);
    splinterdb_cfg.data_cfg = &splinter_data_cfg;

    splinterdb_cfg.use_stats = true;
    splinterdb_cfg.cache_use_stats = true;

    splinterdb *spl_handle = NULL;  // To a running SplinterDB instance

    int rc = splinterdb_create(&splinterdb_cfg, &spl_handle);
    printf("Created SplinterDB instance, dbname '%s'.\n\n", DB_FILE_NAME);

    // Insert a few kv-pairs, describing properties of fruits.
    const char *fruit = "apple";
    const char *descr = "An apple a day keeps the doctor away!";
    slice key = slice_create((size_t)strlen(fruit), fruit);
    slice value = slice_create((size_t)strlen(descr), descr);

    rc = splinterdb_insert(spl_handle, key, value);
    printf("Inserted key '%s'\n", fruit);
    

    splinterdb_print_cache(spl_handle, "cachedump");
    splinterdb_stats_print_insertion(spl_handle);
    splinterdb_stats_print_lookup(spl_handle);

    splinterdb_close(&spl_handle);
    printf("Shutdown SplinterDB instance, dbname '%s'.\n\n", DB_FILE_NAME);

    return rc;
}