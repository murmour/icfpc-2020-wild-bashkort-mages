#pragma once
#include <vector>
#include <functional>

using namespace std;

struct Point
{
	int x = 0;
	int y = 0;
};

struct Ship
{
	Point pos;
	int id;
	//Point vel;
	//int shoot_power;
	int temperature;
	int fuel;
	int laser;
	int cooler;
	int cores;
	bool is_owned;
};

struct Command
{
	bool explode = false;
	bool shoot = false;
	Point shoot_target;
	Point acceleration;
};

enum Role
{
	kAttacker,
	kDefender,
	kUndefined,
};

struct GameState
{
	Role role = kUndefined;
	vector<Ship> my_ships;
	vector<Ship> enemy_ships;
	int ticks_left = -1;
	int ticks_elapsed = -1;
	Point planet_size;
	Point arena_top_left;
	Point arena_bottom_right;
};

typedef function < vector<Command>(const GameState &old_state, const GameState &state) > bot_func;
