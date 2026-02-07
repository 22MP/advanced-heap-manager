#pragma once

#include "Common.h"

// Pokreæe jednog klijenta u posebnoj niti
// id              -> identifikator klijenta (radi logovanja)
// numMessages     -> koliko poruka salje serveru
void RunClient(int id, int numMessages);
