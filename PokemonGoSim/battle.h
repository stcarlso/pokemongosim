#pragma once

#include "pokemon.h"

// Fights once and reports the stats
int fight(BattleResult *result, const Pokemon *attack, const Pokemon *defense, int strategy);
// Fights over and over again and records summary stats
void repeatFight(RepeatBattleResult *result, const Pokemon *attack, const Pokemon *defense,
	int n, int strategy);
