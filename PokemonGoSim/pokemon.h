#pragma once

// All PRINT_xxx switches below will do nothing in Release builds

// If this is enabled, prints out messages for each damage event
#undef PRINT_DAMAGE
// If this is enabled, prints out messages for debugging the attacker plan
#undef PRINT_PLAN
// If this is enabled, prints out messages every time something is queued
#undef PRINT_QUEUES
// If this is enabled, prints out messages at the beginning and end of each battle
#define PRINT_RESULTS
// If this is enabled, prints out messages on every dodge and wait execution
#undef PRINT_ALL_ACTIONS

// Types
#define NO_TYPE 0
#define TYPE_FIG 1
#define TYPE_FLY 2
#define TYPE_POI 3
#define TYPE_GND 4
#define TYPE_ROC 5
#define TYPE_BUG 6
#define TYPE_GHO 7
#define TYPE_STL 8
#define TYPE_FIR 9
#define TYPE_WAT 10
#define TYPE_GRS 11
#define TYPE_ELE 12
#define TYPE_PSY 13
#define TYPE_ICE 14
#define TYPE_DRG 15
#define TYPE_DRK 16
#define TYPE_FRY 17
#define TYPE_NOR 18
#define NUM_TYPES 19

// 75 fully evolved, obtainable mons
#define NUM_SPECIES 75
// Maximum index used for a move
#define MAX_MOVE_INDEX 242
// Maximum number of defender combinations which will be tried
#define MAX_DEFENDERS 512
// How many events to store in the timeline
#define TIMELINE_LEN 512
// Maximum number of learnable basic moves per pokemon
#define MAX_BASIC_MOVES 2
// Maximum number of learnable special moves per pokemon
#define MAX_SPECIAL_MOVES 3

// After this many HP lost, the victim will gain 1 energy
#define HP_TO_ENERGY 2
// Multiplier for STAB
#define MULT_STAB 1.25
// Multiplier for Super Effective (1/x for Not Very Effective)
#define MULT_SUPER 1.25
// Dodging negates 75% of all damage
#define MULT_DODGE 0.25
// Time taken to dodge
#define DODGE_TIME 500
// If we have to wait between our quick attack and the dodge, start dodge this many ms before
// the dodge window closes
#define DODGE_WAIT 200
// Time taken to charge special attack
#define CHARGE_TIME 1000
// Delay for attacker - assuming 0 given that things can be queued
#define ATK_DELAY 0
// Attacker HP multiplier
#define ATK_HP_MULT 1
// Attacker max NRG
#define ATK_NRG_MAX 100
// Delay time on defender
#define DEF_DELAY 2000
// Defender HP multiplier
#define DEF_HP_MULT 2
// Defender max NRG
#define DEF_NRG_MAX 200
// Defender probability of using special if energy available, out of 65536
#define DEF_PROB 32768
// Battle length
#define MAX_TIME 99000

// Dodge nothing
#define STRAT_NO_DODGE 0
// Dodge charged moves only
#define STRAT_DODGE_CHARGE 1
// Dodge all moves
#define STRAT_DODGE_ALL 2

// Nothing
#define EVENT_NOP 0
// Basic attack
#define EVENT_BASIC 1
// Special attack
#define EVENT_SPECIAL 2
// Dodge
#define EVENT_DODGE 3

typedef struct _Species {
	int number;
	// Pointer to dynamically allocated memory
	char *name;
	// Base HP
	int hp;
	// Base Attack
	int attack;
	// Base Defense
	int defense;
	// Types, 0 = no type
	int type[2];
	// Basic moves learnable
	int basic[MAX_BASIC_MOVES];
	// Special moves learnable
	int special[MAX_SPECIAL_MOVES];
} Species;

typedef struct _Pokemon {
	// Index into specData
	int species;
	// Integers only, Poke Go Level = level / 2.0
	int level;
	// Attack IV
	int ivAttack;
	// Defense IV
	int ivDefense;
	// HP IV
	int ivHP;
	// Basic move ID
	int basicMove;
	// Charge move ID
	int powerMove;
} Pokemon;

typedef struct _Move {
	int number;
	// Pointer to dynamically allocated memory
	char *name;
	// Damage
	int power;
	// TYPE_x
	int type;
	// Energy to use
	int energyReq;
	// Energy generated
	int energyGen;
	// Execution time ms
	int cooldown;
	// Time when damage is applied in ms
	int window;
} Move;

typedef struct _BattleResult {
	// Attack pokemon
	const Pokemon *attacking;
	// Defense pokemon
	const Pokemon *defending;
	// Damage done to attacker [includes overkill]
	int atkDamage;
	// Damage done to defender [includes overkill]
	int defDamage;
	// Time left on the battle clock in ms
	int timeLeft;
} BattleResult;

typedef struct _RepeatBattleResult {
	// Attack pokemon
	const Pokemon *attacking;
	// Defense pokemon
	const Pokemon *defending;
	// Average damage done to attacker [includes overkill]
	double avgAtkDamage;
	// Damage done to defender [includes overkill]
	double avgDefDamage;
	// Time left on the battle clock in ms
	double avgTimeLeft;
	// Number of times run
	int ntimes;
	// Number of attacker wins (balance is defender wins or timeouts)
	int atkWins;
} RepeatBattleResult;

typedef struct _FightEvent {
	// Timestamp of occurrence
	int time;
	// Event type
	int type;
	// Event duration
	int duration;
} FightEvent;

typedef struct _Timeline {
	// Timeline data
	FightEvent *data;
	// Current plan index
	int plan;
	// Current execution index
	int exec;
	// Last attack or dodge event executed
	FightEvent *lastAttack;
} Timeline;

// CP multiplier
extern const double CPM[];
// Super effective matrix
extern const int SUPER_EFFECTIVE[NUM_TYPES][NUM_TYPES];
// Species type names
extern const char * const TYPES[];

// All defenders to pit the attackers against
extern Pokemon defenders[];
// Moves by ID
extern Move moves[];
// Global species data filled by readSpecies()
extern Species specData[];

// Clears all events from the timeline object
void clearTimeline(Timeline *timeline);
// Deallocate all non null *name in globals
void destroyAll();
// Destroys the timeline object
void destroyTimeline(Timeline *timeline);
// Calculates the CP of a pokemon
int getCP(const Pokemon *mon);
// Calculates damage of the specified move
int getDamage(const Pokemon *attack, const Pokemon *defense, const Move *move, bool dodge);
// Calculates the HP of a pokemon
int getHP(const Pokemon *mon);
// Gets the move index by its name; case insensitive matching
// Slow! Do not run in a loop!
int getMoveName(const char *name);
// Gets the species index by the # in the pokedex
int getSpeciesNumber(int number);
// Gets the species index by its name; case insensitive matching
// Slow! Do not run in a loop!
int getSpeciesName(const char *name);
// Initializes the timeline object
void initTimeline(Timeline *timeline);
// Prints out a pokemon detail
void printPokemon(const Pokemon *mon);
// Read in basic move data
bool readMovesBasic();
// Read in charge move data
bool readMovesPower();
// Read in species data; must have read moves first!
bool readSpecies();
