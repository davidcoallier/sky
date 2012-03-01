/*
 * Copyright (c) 2012 Ben Johnson, http://skylandlabs.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <inttypes.h>

#include "dbg.h"
#include "bstring.h"
#include "database.h"
#include "object_file.h"

//==============================================================================
//
// Functions
//
//==============================================================================

//======================================
// File Management
//======================================

/**
 * Checks if a file exists.
 */
int file_exists(bstring path)
{
    struct stat buffer;
    int rc = stat(bdata(path), &buffer);
    return rc;
}


//======================================
// Header Management
//======================================

/**
 * Loads the header data into the object file structure.
 */
int load_header(ObjectFile *object_file)
{
    // TODO: If header file does not exist, set empty header data.
    // TODO: Otherwise open header file.
    // TODO: Read object id ranges into structs.
    // TODO: Close header file.
    // TODO: Sort ranges by starting object id.
    
    return 0;
}


//======================================
// Action Management
//======================================

/**
 * Loads action information from file.
 */
int load_actions(ObjectFile *object_file)
{
    int rc;
    FILE *file;
    Action *actions;
    char *buffer;
    uint32_t count = 0;
    
    // Retrieve file stats on actions file
    bstring path = bformat("%s/actions", bdata(object_file->path)); check_mem(path);
    
    // Read in actions file if it exists.
    if(file_exists(path)) {
        file = fopen(bdata(path), "r");
        check(file, "Failed to open action file: %s",  bdata(path));
        
        // Read action count.
        fread(&count, sizeof(count), 1, file);
        actions = malloc(sizeof(Action) * count);
        
        // Read actions until end of file.
        uint32_t i;
        uint16_t length;
        for(i=0; i<count && !feof(file); i++) {
            // Read action id and name length.
            fread(&actions[i].id, sizeof(actions[i].id), 1, file);
            fread(&length, sizeof(length), 1, file);

            // Read action name.
            buffer = calloc(1, length+1); check_mem(buffer);
            check(fread(&buffer, sizeof(length), 1, file) == length, "Corrupt actions file");
            actions[i].name = bfromcstr(buffer); check_mem(actions[i].name);
        }
        
        // Close the file.
        fclose(file);
    }

    // Store action list on object file.
    object_file->actions = actions;
    object_file->action_count = count;
    
    // Clean up.
    bdestroy(path);
    
    return 0;

error:
    if(file) fclose(file);
    if(buffer) free(buffer);
    bdestroy(path);
    return -1;
}

//======================================
// Property Management
//======================================

/**
 * Loads property information from file.
 */
int load_properties(ObjectFile *object_file)
{
    return 0;
}


//======================================
// Locking
//======================================

/**
 * Obtains a write lock on the object file.
 */
int lock(ObjectFile *object_file)
{
    // TODO: Check for lock file in object file directory.
    // TODO: If lock exists, check if owner process still exists.
    // TODO: If owner is gone then remove lock.
    
    // TODO: Write PID to lock file in object file directory.
    
    return 0;
}

/**
 * Removes a lock on the object file obtained by this process.
 */
int unlock(ObjectFile *object_file)
{
    // TODO: Check for lock file in object file directory.
    // TODO: If contents of lock file are this file's PID then remove lock file.

    return 0;
}


//======================================
// Lifecycle
//======================================

/*
 * Creates a reference to an object file.
 *
 * database - A reference to the database that the object file belongs to.
 * name - The name of the object file.
 */
ObjectFile *ObjectFile_create(Database *database, bstring name)
{
    ObjectFile *object_file;
    
    check(database != NULL, "Cannot create object file without a database");
    check(name != NULL, "Cannot create unnamed object file");
    
    object_file = malloc(sizeof(ObjectFile));
    object_file->name = bstrcpy(name); check_mem(object_file->name);
    object_file->path = bformat("%s/%s", bdata(database->path), bdata(object_file->name));
    check_mem(object_file->path);

    object_file->infos = NULL;
    object_file->block_count = 0;

    object_file->actions = NULL;
    object_file->action_count = 0;

    object_file->properties = NULL;
    object_file->property_count = 0;

    return object_file;
    
error:
    ObjectFile_destroy(object_file);
    return NULL;
}

/*
 * Removes an object file reference from memory.
 */
void ObjectFile_destroy(ObjectFile *object_file)
{
    if(object_file) {
        bdestroy(object_file->name);
        bdestroy(object_file->path);
        free(object_file);
    }
}


//======================================
// State
//======================================

/**
 * Opens the object file for reading and writing events.
 */
int ObjectFile_open(ObjectFile *object_file)
{
    // Obtain lock.
    check(lock(object_file) == 0, "Unable to obtain lock");
    
    // Load header, action and properties data.
    check(load_header(object_file) == 0, "Unable to load header data");
    check(load_actions(object_file) == 0, "Unable to load action data");
    check(load_properties(object_file) == 0, "Unable to load property data");
    
    return 0;

error:
    return -1;
}

/**
 * Closes the object file.
 */
int ObjectFile_close(ObjectFile *object_file)
{
    check(unlock(object_file) == 0, "Unable to remove lock");
    return 0;
    
error:
    return -1;
}


//======================================
// Event Management
//======================================

int ObjectFile_add_event(ObjectFile *object_file, Event *event)
{
    // TODO: If there are no blocks then create a block.
    // TODO: Otherwise find block that contains existing event.
    // TODO: If no existing object found, find closest matching block.
    
    // TODO: Find existing object path or create object path.
    // TODO: Insert event into path.
    // TODO: Insert path into block.
    // TODO: If block is larger than limit then split the block into multiple blocks.
    
    // TODO: Write all blocks to disk.
    
    return 0;
}