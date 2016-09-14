#include "stdafx.h"
#include "battle.h"

typedef struct _BattleStatus {
	// Pokemon
	const Pokemon *mon;
	// Initial HP
	int hp;
	// Energy
	int nrg;
	// Damage done to
	int damage;
	// Timeline
	Timeline *tl;
} BattleStatus;

// Reports the next upcoming non-wait event of the timeline; if all the remaining events are
// wait events, reports the time of the last planned event
static FightEvent * planEvent(const Timeline *tl) {
	int idx = tl->exec, max = tl->plan;
	FightEvent *evt;
	do {
		evt = &(tl->data[idx++]);
	} while (evt->type == EVENT_NOP && idx < max);
	return evt;
}

// Adds an event to the timeline
static int addEvent(const BattleStatus *status, int eventType, int duration) {
	const Move *move;
	Timeline *tl = status->tl;
	const Pokemon *user = status->mon;
	int advance = 0, time = 0, pi = tl->plan;
	// Get time of last planned move
	if (pi > 0) {
		FightEvent *last = &(tl->data[pi - 1]);
		time = last->time + last->duration;
	}
	switch (eventType) {
	case EVENT_BASIC:
		// Basic attack
		move = &moves[user->basicMove];
		advance = move->cooldown - move->window;
		break;
	case EVENT_SPECIAL:
		// Special attack
		move = &moves[user->powerMove];
		advance = move->cooldown - move->window;
		break;
	case EVENT_NOP:
	case EVENT_DODGE:
		// Waits for the duration
		advance = duration;
		break;
	default:
		break;
	}
	// Put event in
	FightEvent *at = &(tl->data[pi]);
	at->time = time;
	at->type = eventType;
	at->duration = advance;
	// Move plan index up one (plan points to next empty index)
	tl->plan = pi + 1;
	return time;
}

// Reports the head event of the timeline
static FightEvent * headEvent(const Timeline *tl) {
	return &(tl->data[tl->exec]);
}

// Initializes a battle status with pokemon stats
static void initBattle(BattleStatus *stat, const Pokemon *mon, Timeline *tl, int hpMult) {
	// HP = 2 * (BaseHP + IVHP), defender gets double
	stat->hp = hpMult * getHP(mon);
	stat->damage = 0;
	stat->nrg = 0;
	stat->mon = mon;
	stat->tl = tl;
	tl->data[0].time = 0;
#if defined(PRINT_RESULTS) && defined(_DEBUG)
	printPokemon(mon);
#endif
}

// Dodges the move!
static inline int attackerDodge(BattleStatus *atk) {
	return addEvent(atk, EVENT_DODGE, DODGE_TIME);
}

// Adds an attacker attack of the given type (assumes energy is available)
static inline int attackerDoAttack(BattleStatus *atk, int now, int type) {
	const Pokemon *attack = atk->mon;
	Timeline *atkTL = atk->tl;
	const Move *pwr = &moves[attack->powerMove], *basic = &moves[attack->basicMove];
	int nrg = atk->nrg, when;
#if defined(PRINT_QUEUES) && defined(_DEBUG)
	const char *atkName = specData[attack->species].name;
#endif
	if (type == EVENT_SPECIAL) {
		// Special! Usually the special can be charged during the previous dodge, but add half
		// the charge time to make sure
		when = addEvent(atk, EVENT_NOP, (CHARGE_TIME >> 1) + pwr->window) +
			(CHARGE_TIME >> 1);
		addEvent(atk, EVENT_SPECIAL, 0);
#if defined(PRINT_QUEUES) && defined(_DEBUG)
		printf("[ %05d ] %s queued %s at %d\n", now, atkName, pwr->name, when);
#endif
		nrg -= pwr->energyReq;
	} else {
		// Basic
		when = addEvent(atk, EVENT_NOP, basic->window);
		addEvent(atk, EVENT_BASIC, 0);
#if defined(PRINT_QUEUES) && defined(_DEBUG)
		printf("[ %05d ] %s queued %s at %d\n", now, atkName, basic->name, when);
#endif
		nrg += basic->energyGen;
		if (nrg > ATK_NRG_MAX)
			nrg = ATK_NRG_MAX;
#if ATK_DELAY > 0
		addEvent(atkTL, attack, EVENT_NOP, ATK_DELAY);
#endif
	}
	atk->nrg = nrg;
	return when;
}

// Store what happened (exclude the Victory! screen delay on principle)
static inline int battleResult(BattleResult *setup, int et, BattleStatus *atk,
		BattleStatus *def) {
	int result;
	// Display what happened
#if defined(PRINT_RESULTS) && defined(_DEBUG)
	printf("-- Battle over (%d.%03d s) --\n", et / 1000, et % 1000);
	printf(" %s (%d HP) dealt %d damage\n", specData[setup->attacking->species].name, atk->hp -
		atk->damage, def->damage);
	printf(" %s (%d HP) dealt %d damage\n", specData[setup->defending->species].name, def->hp -
		def->damage, atk->damage);
#endif
	setup->timeLeft = MAX_TIME - et;
	setup->atkDamage = atk->damage;
	setup->defDamage = def->damage;
	// Who won?
	result = 1;
	if (def->damage < def->hp) {
		if (atk->damage > atk->hp)
			// Ties go to the attacker
			result = -1;
		else
			// Timeouts go to the defender
			result = 0;
	}
#if defined(PRINT_RESULTS) && defined(_DEBUG)
	if (result > 0)
		puts("Attacker won!");
	else
		puts("Defender won!");
#endif
	return result;
}

// Adds a random defender delay from DEF_DELAY to DEF_DELAY + DEF_DELAY_RANGE
static inline void defenderAddDelay(BattleStatus *def) {
	//https://www.reddit.com/r/TheSilphRoad/comments/52b453/testing_gym_combat_misconceptions_2/
	unsigned int rnd;
	int delay;
	if (rand_s(&rnd) != 0)
		// Default
		delay = DEF_DELAY_RANGE >> 1;
	else
		// In integer, convert to (0..1) * DEF_DELAY_RANGE
		delay = ((int)(rnd & 0xFFFFU) * DEF_DELAY_RANGE + 0x7FFF) / 0xFFFF;
	addEvent(def, EVENT_NOP, DEF_DELAY + delay);
}

// Adds a defender attack, randomly choosing special if there is enough energy
static inline int defenderAttack(BattleStatus *def, int now) {
	const Pokemon *defense = def->mon;
	const Move *pwr = &moves[defense->powerMove], *basic = &moves[defense->basicMove];
	unsigned int rnd;
	int nrg = def->nrg, need = pwr->energyReq, when;
#if defined(PRINT_QUEUES) && defined(_DEBUG)
	const char *defName = specData[defense->species].name;
#endif
	if (nrg > need && rand_s(&rnd) == 0 && (rnd & 0xFFFF) < DEF_PROB) {
		// Special! Defender does not need to charge, but does wait afterwards
		when = addEvent(def, EVENT_NOP, pwr->window);
		addEvent(def, EVENT_SPECIAL, 0);
#if defined(PRINT_QUEUES) && defined(_DEBUG)
		printf("[ %05d ] %s queued %s at %d\n", now, defName, pwr->name, when);
#endif
		nrg -= need;
	} else {
		when = addEvent(def, EVENT_NOP, basic->window);
		addEvent(def, EVENT_BASIC, 0);
#if defined(PRINT_QUEUES) && defined(_DEBUG)
		printf("[ %05d ] %s queued %s at %d\n", now, defName, basic->name, when);
#endif
		nrg += basic->energyGen;
		if (nrg > DEF_NRG_MAX)
			nrg = DEF_NRG_MAX;
	}
	// Delay after
	//https://www.reddit.com/r/TheSilphRoad/comments/4wzll7/testing_gym_combat_misconceptions
	defenderAddDelay(def);
	def->nrg = nrg;
	return when;
}

// Prepares the initial fixed defender strategy
static inline void defenderStart(BattleStatus *def) {
	const Move *defBasic = &moves[def->mon->basicMove];
	int cd = defBasic->cooldown;
	clearTimeline(def->tl);
	// Defender has a fixed initial strategy, which does generate energy!
	// Starts attack at T=1, starts attack again at T=2 (even if first attack not finished!)
	addEvent(def, EVENT_NOP, 1000 + cd);
	addEvent(def, EVENT_BASIC, 0);
	// 1000 + cd in, need to hit again at 2000 + cd, basic attacks take 0 time
	addEvent(def, EVENT_NOP, 1000);
	addEvent(def, EVENT_BASIC, 0);
	// Third attack was based on a 2s delay after the first one ends (1000+cd), needs to start
	// at 3000+cd, currently at 2000+cd
	addEvent(def, EVENT_NOP, 1000);
	def->nrg = defBasic->energyGen;
}

// Executes the head move of the specified timeline
static int execute(BattleStatus *status, const Pokemon *victim, bool dodge) {
	Timeline *timeline = status->tl;
	const Pokemon *user = status->mon;
	// All information is in the status, how useful!
	int idx = timeline->exec, type, damage = 0;
	FightEvent *evt = &timeline->data[idx];
	const Move *move = NULL;
#if defined(PRINT_ALL_ACTIONS) && defined(_DEBUG)
	const char *name = specData[user->species].name;
#endif
	type = evt->type;
	// Damage the victim if this was an attack move
	if (type == EVENT_BASIC)
		move = &moves[user->basicMove];
	else if (type == EVENT_SPECIAL)
		move = &moves[user->powerMove];
	if (move != NULL) {
#if defined(PRINT_DAMAGE) && defined(_DEBUG)
		// Prefix the damage notification
		printf("[ %05d ] ", evt->time);
#endif
		damage = getDamage(user, victim, move, dodge);
	}
#if defined(PRINT_ALL_ACTIONS) && defined(_DEBUG)
	else if (type == EVENT_DODGE)
		printf("[ %05d ] %s dodged!\n", evt->time, name);
	else
		printf("[ %05d ] %s waits for %d\n", evt->time, name, evt->duration);
#endif
	timeline->exec = idx + 1;
	return damage;
}

// Calculates the next attacker plan
static inline void nextAttackerAttack(BattleStatus *atk, BattleStatus *def, int now,
		int atkStrategy) {
	const Pokemon *attack = atk->mon;
	const Move *basic = &moves[attack->basicMove];
	FightEvent *prevAtk = atk->tl->lastAttack, *nextDef = planEvent(def->tl);
	// Find out how much energy is needed and if we, they have power now
	int attackEnd = prevAtk->time + prevAtk->duration, cutoff = nextDef->time - attackEnd,
		nextType = nextDef->type, lastType = def->tl->lastAttack->type;
	bool defHasNRG = def->nrg >= moves[def->mon->powerMove].energyReq, atkHasNRG =
		atk->nrg >= moves[attack->powerMove].energyReq, dodged = prevAtk->type == EVENT_DODGE;
	if (atkStrategy == STRAT_NO_DODGE)
		// Queue one attack always
		attackerDoAttack(atk, now, atkHasNRG ? EVENT_SPECIAL : EVENT_BASIC);
	else if (cutoff > 0) {
		// Calculate how long until next defender attack and how much we can do before then
		// Subtract one from cutoff to make sure that ties err on the side of caution
		int rate = basic->cooldown + ATK_DELAY, qty = (cutoff - 1) / rate, dodgeTime,
			leftover = cutoff - (rate * qty);
		if (atkHasNRG && nextType != EVENT_SPECIAL && (!defHasNRG || (dodged &&
				lastType == EVENT_SPECIAL)))
			// Queue special attack if defender just used its own or cannot use it
			attackerDoAttack(atk, now, EVENT_SPECIAL);
		else if (atkStrategy == STRAT_DODGE_ALL || nextType == EVENT_SPECIAL) {
			// Attack until next defender attack
			for (int i = 0; i < qty; i++)
				attackerDoAttack(atk, now, EVENT_BASIC);
			// Wait until the damage window starts
			if (leftover > DODGE_WAIT)
				addEvent(atk, EVENT_NOP, leftover - DODGE_WAIT);
			// Dodge
			dodgeTime = attackerDodge(atk);
#if defined(PRINT_PLAN) && defined(_DEBUG)
			printf("[ %05d ] Attacking %d times before dodge at %d\n", now, qty, dodgeTime);
#else
			(void)dodgeTime;
#endif
		} else
			// Just hit them, use special if they cannot possibly have energy
			attackerDoAttack(atk, now, (defHasNRG || !atkHasNRG) ? EVENT_BASIC :
				EVENT_SPECIAL);
	}
}

// Fights the pokemon provided in the battle setup!
static int doFight(BattleResult *setup, int atkStrategy, Timeline *atkTL, Timeline *defTL) {
	BattleStatus atk, def;
	int now = 0, nextAT, nextDT, dd, energy;
	const Pokemon *attack = setup->attacking, *defense = setup->defending;
	FightEvent *nextAtk, *nextDef;
	// Set up battle
	clearTimeline(atkTL);
	initBattle(&atk, attack, atkTL, ATK_HP_MULT);
#if defined(PRINT_RESULTS) && defined(_DEBUG)
	puts("-- VS --");
#endif
	initBattle(&def, defense, defTL, DEF_HP_MULT);
	defenderStart(&def);
	// Battle loop
	while (now < MAX_TIME && atk.damage < atk.hp && def.damage < def.hp) {
		// If planned events are exhausted, plan next attack/defense
		if (defTL->exec >= defTL->plan)
			defenderAttack(&def, now);
		if (atkTL->exec >= atkTL->plan)
			nextAttackerAttack(&atk, &def, now, atkStrategy);
		// Advance to the next event
		nextDef = headEvent(defTL);
		nextDT = nextDef->time;
		nextAtk = headEvent(atkTL);
		nextAT = nextAtk->time;
		if (nextDT >= nextAT) {
			// Move attacker timeline, defender cannot dodge at this time
			if (nextAtk->type != EVENT_NOP)
				atkTL->lastAttack = nextAtk;
			dd = execute(&atk, defense, false);
			// Every 2 damage done, add NRG
			energy = def.nrg + (dd + HP_TO_ENERGY - 1) / HP_TO_ENERGY;
			if (energy > DEF_NRG_MAX)
				energy = DEF_NRG_MAX;
			def.nrg = energy;
			def.damage += dd;
		}
		if (nextDT <= nextAT) {
			bool dodge;
			if (nextDef->type != EVENT_NOP)
				defTL->lastAttack = nextDef;
			// Need to use the previous event as any dodges have been retired!
			nextAtk = atkTL->lastAttack;
			dodge = nextAtk->type == EVENT_DODGE && nextAtk->time + nextAtk->duration > nextDT;
			dd = execute(&def, attack, dodge);
			// Every 2 damage done, add NRG (rounds up)
			energy = atk.nrg + (dd + HP_TO_ENERGY - 1) / HP_TO_ENERGY;
			if (energy > ATK_NRG_MAX)
				energy = ATK_NRG_MAX;
			atk.nrg = energy;
			// Move defender timeline
			nextAT = nextDT;
			// Dodge damage needs to be calculated now!
			if (dodge) {
				dd = dd * MULT_DODGE;
				if (dd < 1) dd = 1;
			}
			atk.damage += dd;
		}
		now = nextAT;
	}
	return battleResult(setup, now, &atk, &def);
}

// Fights over and over again and records summary stats
void repeatFight(RepeatBattleResult *result, const Pokemon *attack, const Pokemon *defense,
		int n, int strategy) {
	BattleResult setup;
	Timeline atkTL, defTL;
	if (result != NULL) {
		// Set up
		int totalAD = 0, totalDD = 0, atkWins = 0;
		// The extra 2bil does help for 10000+ runs!
		unsigned int totalTimeLeft = 0;
		setup.attacking = attack;
		setup.defending = defense;
		initTimeline(&atkTL);
		initTimeline(&defTL);
		// If memory available
		if (atkTL.data != NULL && defTL.data != NULL && n > 0) {
			double nd = (double)n;
			result->ntimes = n;
			// Do it, and do it, and do it...
			for (int i = 0; i < n; i++) {
				atkWins += (doFight(&setup, strategy, &atkTL, &defTL) == 1) ? 1 : 0;
				totalAD += setup.atkDamage;
				totalDD += setup.defDamage;
				totalTimeLeft += (unsigned int)setup.timeLeft;
			}
			// Average and store stats
			result->attacking = attack;
			result->defending = defense;
			result->atkWins = atkWins;
			result->avgAtkDamage = (double)totalAD / nd;
			result->avgDefDamage = (double)totalDD / nd;
			result->avgTimeLeft = (double)totalTimeLeft / nd;
			// Clean up
			destroyTimeline(&atkTL);
			destroyTimeline(&defTL);
		} else
			result->ntimes = 0;
	}
}

// Fights once and reports the stats
int fight(BattleResult *result, const Pokemon *attack, const Pokemon *defense, int strategy) {
	Timeline atkTL, defTL;
	int ret = 0;
	// Set up
	if (result != NULL) {
		result->attacking = attack;
		result->defending = defense;
		initTimeline(&atkTL);
		initTimeline(&defTL);
		// If memory available
		if (atkTL.data != NULL && defTL.data != NULL) {
			ret = doFight(result, strategy, &atkTL, &defTL);
			// Clean up
			destroyTimeline(&atkTL);
			destroyTimeline(&defTL);
		}
	}
	return ret;
}
