// Copyright 2022 VMware, Inc.
// SPDX-License-Identifier: Apache-2.0
/*
 * SplinterDB "Hello World" example program. Demonstrate use of:
 *  - Basic SplinterDB configuration and create() interface
 *  - Insert, lookup and iterator interfaces
 *  - Close and reopen a SplinterDB db (instance)
 */

#include <cstring>
#include <string>

extern "C" {
#define _Bool int
#include "splinterdb/default_data_config.h"
#include "splinterdb/splinterdb.h"
}

#define DB_FILE_NAME "splinterdb_intro_db"
#define DB_FILE_SIZE_MB 1024  // Size of SplinterDB device; Fixed when created
#define CACHE_SIZE_MB 1      // Size of cache; can be changed across boots

/* Application declares the limit of key-sizes it intends to use */
#define USER_MAX_KEY_SIZE ((int)24)

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
    std::string key = "user11111111111111111111";
    std::string value(1434, 'a');
    slice key_slice = slice_create(key.size(), key.data());
    slice value_slice = slice_create(value.size(), value.data());

    rc = splinterdb_insert(spl_handle, key_slice, value_slice);
    printf("Inserted key '%s' rc=%d\n", key.data(), rc);

    key = "user22222222222222222222";
    key_slice = slice_create(key.size(), key.data());
    rc = splinterdb_insert(spl_handle, key_slice, value_slice);
    printf("Inserted key '%s' rc=%d\n", key.data(), rc);

    for (size_t i = 0; i < 1; i++) {
        splinterdb_lookup_result result;
        splinterdb_lookup_result_init(spl_handle, &result, 0, NULL);
        int retcode = splinterdb_lookup(spl_handle, key_slice, &result);

        slice value_ret;
        retcode = splinterdb_lookup_result_value(&result, &value_ret);
        std::string output(static_cast<const char*>(value_ret.data),
                        static_cast<size_t>(value_ret.length));
        printf("got value of length %zu\n", output.size());
    }

    splinterdb_print_cache(spl_handle, "cachedump");
    // splinterdb_stats_print_insertion(spl_handle);
    // splinterdb_stats_print_lookup(spl_handle);

    splinterdb_close(&spl_handle);
    printf("Shutdown SplinterDB instance, dbname '%s'.\n\n", DB_FILE_NAME);

    return rc;
}