#ifndef _sky_next_action_message_h
#define _sky_next_action_message_h

#include <inttypes.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "bstring.h"
#include "table.h"
#include "event.h"


//==============================================================================
//
// Typedefs
//
//==============================================================================

// A message for retrieving a count of the next immediate action following a
// series of actions.
typedef struct sky_next_action_message {
    sky_action_id_t *prior_action_ids;
    uint32_t prior_action_id_count;
} sky_next_action_message;


//==============================================================================
//
// Functions
//
//==============================================================================

//--------------------------------------
// Lifecycle
//--------------------------------------

sky_next_action_message *sky_next_action_message_create();

void sky_next_action_message_free(sky_next_action_message *message);

void sky_next_action_message_free_deps(sky_next_action_message *message);

//--------------------------------------
// Serialization
//--------------------------------------

int sky_next_action_message_pack(sky_next_action_message *message,
    FILE *file);

int sky_next_action_message_unpack(sky_next_action_message *message,
    FILE *file);

//--------------------------------------
// Processing
//--------------------------------------

int sky_next_action_message_process(sky_next_action_message *message,
    sky_table *table, FILE *output);

#endif
