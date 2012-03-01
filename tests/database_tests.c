#include <stdio.h>
#include <database.h>
#include <bstring.h>

#include "minunit.h"

//==============================================================================
//
// Test Cases
//
//==============================================================================

char *test_Database_create_destroy() {
    Database *database = Database_create(
        bfromcstr("/etc/sky/data")
    );
    mu_assert(database != NULL, "Could not create database");
    mu_assert(biseqcstr(database->path, "/etc/sky/data"), "Invalid path");

    Database_destroy(database);

    return NULL;
}



//==============================================================================
//
// Setup
//
//==============================================================================

char *all_tests() {
    mu_run_test(test_Database_create_destroy);
    return NULL;
}

RUN_TESTS(all_tests)