#pragma once

#include "Common.h"

// Pokrece server (slusa klijente i obradjuje poruke)
// useAHM -> true = koristi AHM, false = koristi malloc/free
void RunServer(bool useAHM);
