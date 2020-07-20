#pragma once
#include <vector>

using namespace std;

struct Command;
struct GameState;

vector<Command> bot_func_icarus(const GameState &old_state, const GameState &state);
vector<Command> bot_func_sniper(const GameState &old_state, const GameState &state);
vector<Command> bot_func_kamikaze(const GameState &old_state, const GameState &state);
vector<Command> bot_func_kamikaze2(const GameState &old_state, const GameState &state);
vector<Command> bot_func_combine(const GameState &old_state, const GameState &state);
vector<Command> bot_func_assassin(const GameState &old_state, const GameState &state);
vector<Command> bot_func_combine2(const GameState &old_state, const GameState &state);
vector<Command> bot_func_combine3(const GameState &old_state, const GameState &state);
