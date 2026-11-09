#pragma once

#include "Common.h"

// Starts the server (listens for clients and processes messages)
// useAHM -> true = uses AHM, false = uses malloc/free
void RunServer(bool useAHM);