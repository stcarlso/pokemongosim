# pokemongosim

A simulator for Pokemon Go battles. Runs offline in C++, no references to the API; experiment
as often as you would like!

## Purpose

Charts such as http://imgur.com/a/tnxxq suggest top tier attackers and defenders to use against
gyms in Pokemon Go. However, these charts often rely on raw DPS and fail to take into account
attacker strategies involving dodging. This program simulates a complete battle timeline on
a second to second basis to more accurately get a picture of battle events.

Attackers can simply hammer away without dodging, intelligently dodge only special moves from
the defender while timing their own to minimize the chance of getting hit, or dodge everything
while weaving in the perfect number of basic and special attacks to mitigate damage as much as
possible.

## Compiling

Open PokemonGoSim.sln in Visual Studio 2015.

Compiling should be fairly straightforward after that. The "Debug" configuration will emit
thousands of diagnostic messages, unless they are disabled in the pokemon.h file. "Release" will
attempt to automatically parallelize the simulation loop, which makes going through hundreds
of attackers significantly faster.
