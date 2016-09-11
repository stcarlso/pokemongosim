#include "stdafx.h"
#include "pokemon.h"

// Assuming dense packing?
#define TIMELINE_BYTES (sizeof(FightEvent) * (size_t)TIMELINE_LEN)

// Flags for super effective / not very effective
#define S 1
#define N 2

// Popup text if an attack hits
#ifdef _DEBUG
static const char * EFFECT_TEXT[5] = {
	"Not Very Effective *2",  "Not Very Effective", "Normal", "Super Effective",
	"Super Effective *2"
};
#endif

// Type names
const char * const TYPES[NUM_TYPES] = {
	"None", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost", "Steel",
	"Fire", "Water", "Grass", "Electric", "Psychic", "Ice", "Dragon", "Dark", "Fairy",
	"Normal"
};

// CP multipliers
const double CPM[] = {
	0.0,
	// 1
	0.09399999678134918,
	0.1351374313235283,
	0.16639786958694458,
	0.1926509141921997,
	0.21573247015476227,
	0.23657265305519104,
	0.2557200491428375,
	0.27353037893772125,
	// 5
	0.29024988412857056,
	0.3060573786497116,
	0.3210875988006592,
	0.33544503152370453,
	0.3492126762866974,
	0.362457737326622,
	0.37523558735847473,
	0.38759241108516856,
	0.39956727623939514,
	0.4111935495172506,
	// 10
	0.42250001430511475,
	0.4329264134104144,
	0.443107545375824,
	0.45305995387198583,
	0.46279838681221,
	0.4723360762000084,
	0.4627983868122101,
	0.48168496231674496,
	0.49985843896865845,
	0.5087017569439922,
	// 15
	0.517393946647644,
	0.5259425087713293,
	0.5343543291091919,
	0.5426357622303539,
	0.5507926940917969,
	0.5588305993005633,
	0.5667545199394226,
	0.574569147080183,
	0.5822789072990417,
	0.5898879119195044,
	// 20
	0.5974000096321106,
	0.6048236563801765,
	0.6121572852134705,
	0.6194041110575199,
	0.6265671253204346,
	0.633649181574583,
	0.6406529545783997,
	0.6475809663534164,
	0.654435634613037,
	0.6612192690372467,
	// 25
	0.667934000492096,
	0.6745819002389908,
	0.6811649203300476,
	0.6876849085092545,
	0.6941436529159546,
	0.7005428969860077,
	0.7068842053413391,
	0.7131690979003906,
	0.719399094581604,
	0.7255756169725986,
	// 30
	0.731700003147125,
	0.7347410088680817,
	0.73776948,
	0.74078557,
	0.74378943,
	0.746781204,
	0.74976104,
	0.752729104,
	0.75568551,
	0.75863037,
	// 35
	0.76156384,
	0.76448607,
	0.76739717,
	0.77029727,
	0.7731865,
	0.77606494,
	0.77893275,
	0.78179006,
	0.78463697,
	0.78747358,
	// 40
	0.79030001
};

// [A][D] super effective matrix
const int SUPER_EFFECTIVE[NUM_TYPES][NUM_TYPES] = {
	//F L P G R B O S F W G E P I D K Y N
	{ 0,N,N,0,S,N,N,S,0,0,0,0,N,S,0,S,N,S }, // FIG
	{ S,0,0,0,N,S,0,N,0,0,S,N,0,0,0,0,0,0 }, // FLY
	{ 0,0,N,N,N,0,N,N,0,0,S,0,0,0,0,0,S,0 }, // POI
	{ 0,0,N,S,0,S,N,S,S,0,N,S,0,0,0,0,0,0 }, // GND
	{ N,S,0,N,0,S,0,N,S,0,0,0,0,S,0,0,0,0 }, // ROC
	{ N,N,N,0,0,0,N,N,N,0,S,0,S,0,0,S,N,0 }, // BUG
	{ 0,0,0,0,0,0,S,0,0,0,0,0,S,0,0,0,0,N }, // GHO
	{ 0,0,0,0,S,0,0,N,N,N,0,N,0,S,0,0,S,0 }, // STL
	{ 0,0,0,0,N,S,0,S,N,N,S,0,0,S,N,0,0,0 }, // FIR
	{ 0,0,0,S,S,0,0,0,S,N,N,0,0,0,N,0,0,0 }, // WAT
	{ 0,N,N,S,S,N,0,N,N,S,N,0,0,0,N,0,0,0 }, // GRS
	{ 0,S,0,N,0,0,0,0,0,S,N,N,0,0,N,0,0,0 }, // ELE
	{ S,0,S,0,0,0,0,N,0,0,0,0,N,0,0,N,0,0 }, // PSY
	{ 0,S,0,S,0,0,0,N,N,N,S,0,0,N,S,0,0,0 }, // ICE
	{ 0,0,0,0,0,0,0,N,0,0,0,0,0,0,S,0,N,0 }, // DRG
	{ N,0,0,0,0,0,S,0,0,0,0,0,S,0,0,N,N,0 }, // DRK
	{ S,0,N,0,0,0,0,N,N,0,0,0,0,0,S,S,0,0 }, // FRY
	{ 0,0,0,0,N,0,N,N,0,0,0,0,0,0,0,0,0,0 }  // NOR
};

Pokemon defenders[MAX_DEFENDERS];
Move moves[MAX_MOVE_INDEX];
Species specData[NUM_SPECIES];

// Reports the effectiveness of the move on this species as a number of type advantages
static inline int getEffectiveness(const Move *move, const Species *victim) {
	int total = 0, type0 = victim->type[0], type1 = victim->type[1], eff1 = 0, eff2 = 0,
		mt = move->type - 1;
	// Moves cannot have null types, but pokemon can
	if (type0 > 0)
		eff1 = SUPER_EFFECTIVE[mt][type0 - 1];
	if (type1 > 0)
		eff2 = SUPER_EFFECTIVE[mt][type1 - 1];
	// Add advantages
	if (eff1 == S)
		total++;
	else if (eff1 == N)
		total--;
	if (eff2 == S)
		total++;
	else if (eff2 == N)
		total--;
	return total;
}

// Reports true if the move gets STAB on this pokemon
static inline bool isSTAB(const Move *move, const Species *attack) {
	return attack->type[0] == move->type || attack->type[1] == move->type;
}

// Copies the name from a temporary buffer to dynamic memory, returning the new copy
static char * copyName(const char *buffer, int maxLen) {
	char *name = NULL;
	// Allocate new name
	size_t len = strlen(buffer);
	if (len > 0 && len <= maxLen) {
		name = (char *)malloc(len + 1U);
		if (name != NULL)
			// Has memory
			strcpy_s(name, len + 1U, buffer);
	}
	return name;
}

#ifdef _DEBUG
// Outputs a list of moves by ID
static void printMoveList(const int *data, int count) {
	printf("[");
	for (int j = 0; j < count; j++)
		if (data[j] > 0) {
			printf("%s", moves[data[j]].name);
			if (j < count - 1)
				// Only if necessary
				printf(", ");
		}
	puts("]");
}
#endif

// Clears all events from the timeline object
void clearTimeline(Timeline *timeline) {
	memset(timeline->data, 0, TIMELINE_BYTES);
	timeline->exec = 0;
	timeline->plan = 0;
	// Not always an attack, but not null...
	timeline->lastAttack = timeline->data;
}

// Deallocate all non null *name in globals
void destroyAll() {
	for (int i = 0; i < NUM_SPECIES; i++) {
		char *name = specData[i].name;
		if (name != NULL) {
			// Deallocate
			free(name);
			specData[i].name = NULL;
		}
	}
	for (int i = 0; i < MAX_MOVE_INDEX; i++) {
		char *name = moves[i].name;
		if (name != NULL) {
			// Deallocate
			free(name);
			moves[i].name = NULL;
		}
	}
}

// Destroys the timeline object
void destroyTimeline(Timeline *timeline) {
	FightEvent *data = timeline->data;
	if (data != NULL) {
		free(data);
		timeline->data = NULL;
	}
}

// Calculates the CP of a pokemon
int getCP(const Pokemon *mon) {
	Species *spec = &specData[mon->species];
	double atk = (double)(spec->attack + mon->ivAttack);
	double def = sqrt((double)(spec->defense + mon->ivDefense));
	double hp = sqrt((double)(spec->hp + mon->ivHP));
	double cpml = CPM[mon->level];
	int cp = (int)floor(atk * def * hp * (cpml * cpml) * 0.1);
	// Minimum 10 CP
	if (cp < 10) cp = 10;
	return cp;
}

// Calculates damage of the specified move
int getDamage(const Pokemon *attack, const Pokemon *defense, const Move *move, bool dodge) {
	/* Attacker's Attack = ( base_attack + attack_IV ) * CPM
	* Defender's Defense = ( base_defense + defense_IV ) * CPM
	* Damage = Floor(.5 Attack / Defense * Power * STAB * Weakness) + 1
	*/
	Species *atkSpec = &specData[attack->species], *defSpec = &specData[defense->species];
	int effective = getEffectiveness(move, defSpec);
	double att = (attack->ivAttack + atkSpec->attack) * CPM[attack->level];
	double def = (defense->ivDefense + defSpec->defense) * CPM[defense->level];
	double multipliers = (isSTAB(move, atkSpec) ? MULT_STAB : 1.00);
	switch (effective) {
	case 1:
		// It's super effective!
		multipliers *= MULT_SUPER;
		break;
	case 2:
		// It's super effective!
		multipliers *= (MULT_SUPER * MULT_SUPER);
		break;
	case -1:
		// It's not very effective...
		multipliers *= 1.0 / MULT_SUPER;
		break;
	case -2:
		// It's not very effective...
		multipliers *= 1.0 / (MULT_SUPER * MULT_SUPER);
		break;
	default:
		// Max 2 advantages
		break;
	}
	int damage = 1 + (int)floor(0.5 * move->power * att * multipliers * (dodge ? MULT_DODGE :
		1.0) / def);
#if defined(PRINT_DAMAGE) && defined(_DEBUG)
	printf("%d to %s - %s - %s\n", damage, defSpec->name, move->name, dodge ? "Dodged!" :
		EFFECT_TEXT[effective + 2]);
#endif
	return damage;
}

// Calculates the HP of a pokemon
int getHP(const Pokemon *mon) {
	Species *spec = &specData[mon->species];
	int hp = (int)floor((double)(spec->hp + mon->ivHP) * CPM[mon->level]);
	// Minimum 10 HP
	if (hp < 10) hp = 10;
	return hp;
}

// Gets the move index by its name; case insensitive matching
// Slow! Do not run in a loop!
int getMoveName(const char *name) {
	int move = -1;
	for (int i = 0; i < MAX_MOVE_INDEX && move < 0; i++) {
		Move *mv = &moves[i];
		if (mv->name != NULL && _strcmpi(name, mv->name) == 0)
			move = i;
	}
	return move;
}

// Gets the species index by the # in the pokedex
int getSpeciesNumber(int number) {
	int l = 0, r = NUM_SPECIES - 1, species = -1;
	do {
		// Binary search
		int m = (l + r) >> 1, num = specData[m].number;
		if (num == number)
			species = m;
		else if (num < number)
			r = m - 1;
		else
			l = m + 1;
	} while (l <= r && species < 0);
	return species;
}

// Gets the species index by its name; case insensitive matching
// Slow! Do not run in a loop!
int getSpeciesName(const char *name) {
	int species = -1;
	for (int i = 0; i < NUM_SPECIES && species < 0; i++) {
		Species *spec = &specData[i];
		if (spec->name != NULL && _strcmpi(name, spec->name) == 0)
			species = i;
	}
	return species;
}

// Initializes the timeline object
void initTimeline(Timeline *timeline) {
	FightEvent *data = (FightEvent *)malloc(TIMELINE_BYTES);
	timeline->data = data;
	if (data != NULL)
		clearTimeline(timeline);
}

// Prints out a pokemon detail
void printPokemon(const Pokemon *mon) {
	const Species *spec = &specData[mon->species];
	const Move *basic = &moves[mon->basicMove], *power = &moves[mon->powerMove];
	printf("%s [%s/%s] %d HP\n", spec->name, TYPES[spec->type[0]], TYPES[spec->type[1]],
		getHP(mon));
	printf(" CP %d [%d/%d/%d] %s / %s\n", getCP(mon), mon->ivAttack, mon->ivDefense,
		mon->ivHP, basic->name, power->name);
}

// Read in basic move data
bool readMovesBasic() {
	FILE *fh;
	bool done = false;
	if (fopen_s(&fh, "movesBasic.txt", "r") != 0 || fh == NULL)
		// Uh oh
		puts("Failed to load basic move data!\r");
	else {
		int idx;
		Move *move;
		char buffer[80];
		unsigned int sz = (unsigned int)(sizeof(buffer) - 1);
		for (int i = 0; i < MAX_MOVE_INDEX && !feof(fh); i++)
			// Read in all move data
			if (1 == fscanf_s(fh, "%d ", &idx) && idx < MAX_MOVE_INDEX) {
				move = &moves[idx];
				move->energyReq = 0;
				if (5 == fscanf_s(fh, "%[^\t] %d %d %d %d", buffer, sz, &(move->type),
						&(move->power), &(move->cooldown), &(move->energyGen))) {
					move->name = copyName(buffer, 80);
#ifdef _DEBUG
					printf("MOVE %s [%s] P=%d T=%d E=%d\n", move->name, TYPES[move->type],
						move->power, move->cooldown, move->energyGen);
#endif
					move->window = move->cooldown;
				}
			}
		fclose(fh);
		done = true;
	}
	return done;
}

// Read in charge move data
bool readMovesPower() {
	FILE *fh;
	bool done = false;
	if (fopen_s(&fh, "movesPower.txt", "r") != 0 || fh == NULL)
		// Uh oh
		puts("Failed to load charge move data!\r");
	else {
		int idx;
		Move *move;
		char buffer[80];
		unsigned int sz = (unsigned int)(sizeof(buffer) - 1);
		for (int i = 0; i < MAX_MOVE_INDEX && !feof(fh); i++)
			// Read in all move data
			if (1 == fscanf_s(fh, "%d ", &idx) && idx < MAX_MOVE_INDEX) {
				move = &moves[idx];
				move->energyGen = 0;
				if (6 == fscanf_s(fh, "%[^\t] %d %d %d %d %d", buffer, sz, &(move->type),
						&(move->power), &(move->cooldown), &(move->energyReq),
						&(move->window))) {
					move->name = copyName(buffer, 80);
#ifdef _DEBUG
					printf("MOVE %s [%s] P=%d T=%d E=%d\n", move->name, TYPES[move->type],
						move->power, move->cooldown, move->energyReq);
#endif
				}
			}
		fclose(fh);
		done = true;
	}
	return done;
}

// Read in species data; must have read moves first!
bool readSpecies() {
	FILE *fh;
	bool done = false;
	if (fopen_s(&fh, "species.txt", "r") != 0 || fh == NULL)
		// Uh oh
		puts("Failed to load species data!\r");
	else {
		char buffer[80];
		unsigned int sz = (unsigned int)(sizeof(buffer) - 1);
		for (int i = 0; i < NUM_SPECIES && !feof(fh); i++) {
			// Read in all species data
			Species *mon = &specData[i];
			if (7 == fscanf_s(fh, "%[^\t] %d %d %d %d %d %d ", buffer, sz, &(mon->number),
					&(mon->hp), &(mon->attack), &(mon->defense), &(mon->type[0]),
					&(mon->type[1])))
				mon->name = copyName(buffer, 80);
			else
				mon->name = NULL;
			// Read learnset
			for (int j = 0; j < MAX_BASIC_MOVES && !feof(fh) && 1 == fscanf_s(fh, "%d ",
				&(mon->basic[j])); j++);
			for (int j = 0; j < MAX_SPECIAL_MOVES && !feof(fh) && 1 == fscanf_s(fh, "%d ",
				&(mon->special[j])); j++);
#ifdef _DEBUG
			if (mon->name != NULL) {
				printf("SPEC %s [%s/%s] A=%d D=%d S=%d\n", mon->name, TYPES[mon->type[0]],
					TYPES[mon->type[1]], mon->attack, mon->defense, mon->hp);
				// Report learn set
				printf(" Basic: ");
				printMoveList((const int *)&(mon->basic), MAX_BASIC_MOVES);
				printf(" Charge:");
				printMoveList((const int *)&(mon->special), MAX_SPECIAL_MOVES);
			}
#endif
		}
		fclose(fh);
		done = true;
	}
	return done;
}
