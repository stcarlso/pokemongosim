#include "stdafx.h"
#include "battle.h"
#include "pokemon.h"

// The maximum number of movesets available for a mon (could have fewer)
#define MAX_TOTAL_MOVES (MAX_SPECIAL_MOVES * MAX_BASIC_MOVES)
// Gets the name of a move
#define MOVE_NAME(_species, _type, _value) (moves[(_species)->_type[(_value)]].name)

static void dumpStats(RepeatBattleResult *result) {
	// Percentage won
	int n = result->ntimes, pctWin = (100 * result->atkWins + (n >> 1)) / n;
	printf("-- Battled %d times between:\n", n);
	printPokemon(result->attacking);
	puts("-- and");
	printPokemon(result->defending);
	printf("\n-- Attacker won %d/%d (%d%%)\n", result->atkWins, n, pctWin);
	printf("   Average damage done by attacker: %.1f / %d\n", result->avgDefDamage,
		getHP(result->defending) * DEF_HP_MULT);
	printf("   Average damage done by defender: %.1f / %d\n", result->avgAtkDamage,
		getHP(result->attacking) * ATK_HP_MULT);
	printf("   Average time on battle clock   : %.1f s\n\n", result->avgTimeLeft * 0.001);
}

// Creates a 10/10/10 level 20 (41 by our standards) pokemon of the given name
static void createL20Poke(Pokemon *mon, const char *name, const char *basic,
		const char *special) {
	mon->species = getSpeciesName(name);
	mon->level = 41;
	mon->ivAttack = 10;
	mon->ivDefense = 10;
	mon->ivHP = 10;
	mon->basicMove = getMoveName(basic);
	mon->powerMove = getMoveName(special);
}

// Fills in all possible defenders (movesets) in the defenders array
static void generateAllDefenders() {
	Pokemon *mon = &defenders[0];
	int bm, sm;
	for (int i = 0; i < NUM_SPECIES; i++)
		for (int j = 0; j < MAX_TOTAL_MOVES; j++) {
			// Set up species, level 20, 10/10/10
			Species *spec = &specData[i];
			bm = spec->basic[j / MAX_SPECIAL_MOVES];
			sm = spec->special[j % MAX_SPECIAL_MOVES];
			// Do not generate invalid defenders
			if (bm > 0 && sm > 0) {
				mon->species = i;
				mon->level = 41;
				mon->ivAttack = 10;
				mon->ivDefense = 10;
				mon->ivHP = 10;
				// Pick the moves
				mon->basicMove = bm;
				mon->powerMove = sm;
				mon++;
			}
		}
}

// Fills in all possible movesets for one defender at the specified location in the defenders
// array
static void generateOneDefender(const char *name, int offset) {
	int bm, sm, species = getSpeciesName(name);
	Species *spec = &specData[species];
	Pokemon *mon = &defenders[offset];
	for (int j = 0; j < MAX_TOTAL_MOVES; j++) {
		// Set up species, level 20, 10/10/10
		bm = spec->basic[j / MAX_SPECIAL_MOVES];
		sm = spec->special[j % MAX_SPECIAL_MOVES];
		// Do not generate invalid defenders
		if (bm > 0 && sm > 0) {
			mon->species = species;
			mon->level = 41;
			mon->ivAttack = 10;
			mon->ivDefense = 10;
			mon->ivHP = 10;
			// Pick the moves
			mon->basicMove = bm;
			mon->powerMove = sm;
			mon++;
		}
	}
}

// Reads stored pokemon into the defenders array, using createL20Poke
static int readInMons(const char *filename, int offset, int maxCount) {
	FILE *fh;
	int i = 0;
	if (fopen_s(&fh, filename, "r") != 0 || fh == NULL)
		// Uh oh
		puts("Failed to load saved pokemon list!\r");
	else {
		char name[BUFFER_SIZE], basicMove[BUFFER_SIZE], chargeMove[BUFFER_SIZE];
		Pokemon *def = &defenders[offset];
		for (; i < maxCount && !feof(fh); i++)
			// Read in mon name and moves
			if (3 == fscanf_s(fh, "%[^\t] %[^\t] %[^\n] ", name, BUFFER_SIZE, basicMove,
					BUFFER_SIZE, chargeMove, BUFFER_SIZE)) {
				createL20Poke(def++, name, basicMove, chargeMove);
#if 1
				printf("SAVED %s [%s / %s]\n", name, basicMove, chargeMove);
#endif
			}
		fclose(fh);
	}
	// # actually read
	return i;
}

// Prompts the user for the base pokemon to use (finds first entry in defenders with name
// matching this one, or -1 if none were found)
static int getBasePokemon() {
	char name[80];
	int base = -1, species;
	size_t len;
	// Get the mon name to try from stdin
	printf("Defender name: ");
	fflush(stdout);
	fgets(name, 79, stdin);
	len = strlen(name);
	// Strip the newline
	if (len > 0) {
		name[len - 1] = '\0';
		// Retrieve species
		species = getSpeciesName(name);
		if (species >= 0) {
			// Found it
#if 0
			printf("Matched species: %s (index %d)\n", specData[species].name, species);
#endif
			for (int i = 0; i < MAX_DEFENDERS && base < 0; i++)
				if (defenders[i].species == species)
					base = i;
#ifdef _DEBUG
			printf("Defender index: %d\n", base);
#endif
		}
	}
	return base;
}

// Outputs the battle results (expecting 36) in a matrix
static void printMatrix(RepeatBattleResult *result) {
	Species *atkSpec = &specData[result->attacking->species], *defSpec = &specData[
		result->defending->species];
	// Generate the matrix
	puts("\nKEY:");
	for (int i = 0; i < MAX_TOTAL_MOVES; i++) {
		int fast = i / MAX_SPECIAL_MOVES, charge = i % MAX_SPECIAL_MOVES;
		// Legend to moves
		printf("%1d%1d = %s / %s,  %s / %s\n", fast, charge, MOVE_NAME(atkSpec, basic, fast),
			MOVE_NAME(atkSpec, special, charge), MOVE_NAME(defSpec, basic, fast),
			MOVE_NAME(defSpec, special, charge));
	}
	puts("\nTOTAL:\nAT 00    01    02    10    11    12    DEFENDER");
	for (int i = 0; i < MAX_TOTAL_MOVES; i++) {
		double rt = 0.0, value;
		printf("%1d%1d ", i / MAX_SPECIAL_MOVES, i % MAX_SPECIAL_MOVES);
		for (int j = 0; j < MAX_TOTAL_MOVES; j++) {
			// Show average attack damage to 1 decimal
			value = result[MAX_TOTAL_MOVES * i + j].avgAtkDamage;
			printf("%5.1f ", value);
			rt += value;
		}
		// Row average
		printf("%5.1f\n", rt / (double)MAX_TOTAL_MOVES);
	}
	// Column averages
	printf("-- ");
	for (int i = 0; i < MAX_TOTAL_MOVES; i++) {
		double ct = 0.0;
		for (int j = 0; j < MAX_TOTAL_MOVES; j++)
			ct += result[MAX_TOTAL_MOVES * j + i].avgAtkDamage;
		// Show average attack damage to 1 decimal
		printf("%5.1f ", ct / (double)MAX_TOTAL_MOVES);
	}
	puts("\n");
}

int main() {
	RepeatBattleResult result[150];
	Timeline atkTL, defTL;
	// Read in all data and build timeline objects
	initTimeline(&atkTL);
	initTimeline(&defTL);
	if (readMovesBasic() && readMovesPower() && readSpecies() && atkTL.data != NULL &&
			defTL.data != NULL) {
		// Create top mons
		int base, attackers;
		readInMons("defenders.txt", 0, 150);
		attackers = readInMons("attackers.txt", 200, 150);
		base = getBasePokemon();
		if (base >= 0 && attackers > 0) {
			// Send elite attackers against the defender
			for (int i = 0; i < MAX_TOTAL_MOVES; i++) {
				const Pokemon *defense = &defenders[i + base];
				double td = 0.0, total = (double)attackers;
				unsigned long long totalWins = 0ULL;
				printf("%s has %s / %s...\n", specData[defense->species].name,
					moves[defense->basicMove].name, moves[defense->powerMove].name);
				// This loop can be parallelized, and does it well!
#ifndef _DEBUG
#pragma loop(hint_parallel(8))
#pragma loop(ivdep)
#endif
				for (int j = 0; j < attackers; j++) {
					// Hit against this moveset
#ifdef _DEBUG
					printf("%24s VS %24s...\r", specData[defenders[200 + j].species].name,
						specData[defense->species].name);
					fflush(stdout);
#endif
					// Change this one to determine dodging strategy
					repeatFight(&result[j], &defenders[200 + j], defense, 50000,
						STRAT_DODGE_CHARGE);
				}
				for (int j = 0; j < attackers; j++) {
					// Summary stats
					td += result[j].avgAtkDamage;
					totalWins += (unsigned long long)result[j].atkWins;
				}
				printf("\nAverage damage done to attacker: %.1f (%.2f%% loss rate)\n",
					td / total, 100.0 * (double)totalWins / (50000.0 * total));
			}
		}
		// Done!
		puts("Press ENTER to exit");
		getchar();
		destroyTimeline(&atkTL);
		destroyTimeline(&defTL);
		destroyAll();
	}
	return 0;
}
