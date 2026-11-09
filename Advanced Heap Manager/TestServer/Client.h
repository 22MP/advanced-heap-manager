#pragma once

#include "Common.h"

// Runs a single client in a separate thread
// id              -> client identifier (used for logging)
// numMessages     -> number of messages sent to the server
void RunClient(int id, int numMessages);