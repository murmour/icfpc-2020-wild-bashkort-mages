#define let auto

#include <iostream>
#include <algorithm>
#include <map>
#include "botapi.mli"
#include "bot.mli"

struct MyShip
{
	Ship s;
	Point vel;
};

Ship kill_ship( Ship s )
{
	s.fuel = 0;
	s.laser = 0;
	s.cooler = 0;
	s.cores = 0;
	s.temperature = 0;
	return s;
}

int get_square( const Ship & s )
{
	return s.fuel + s.laser + s.cooler + s.cores;
}

MyShip predict_ship_state( Ship s, Point vel, const Point & pp, const Command & c )
{
	int fu = abs(c.acceleration.x);
	if (abs(c.acceleration.y) > fu) fu = abs(c.acceleration.y);
	if (fu <= s.fuel)
	{
		s.fuel -= fu;
		s.temperature += fu*8;
		vel.x += c.acceleration.x;
		vel.y += c.acceleration.y;
	}
	// gravity
	if (pp.x >= 0)
	{
		if (abs(s.pos.x) == abs(s.pos.y))
		{
			vel.x += (s.pos.x > 0 ? -1 : 1);
			vel.y += (s.pos.y > 0 ? -1 : 1);
		}
		else
		{
			if (s.pos.x > abs(s.pos.y)) vel.x--;
			else if (s.pos.x < -abs(s.pos.y)) vel.x++;
			else if (s.pos.y > abs(s.pos.x)) vel.y--;
			else if (s.pos.y < -abs(s.pos.x)) vel.y++;
		}
	}
	s.pos.x += vel.x;
	s.pos.y += vel.y;

	if (-pp.x <= s.pos.x && s.pos.x <= pp.x && -pp.y <= s.pos.y && s.pos.y <= pp.y)
		return { kill_ship( s ), vel }; // falling to planet

	if (c.explode)
		return { kill_ship(s), vel }; // BOOM! (we don't shoot if exploded)

	// shoot
	s.temperature += s.laser;

	s.temperature -= s.cooler;
	if (s.temperature < 0) s.temperature = 0;
	if (s.temperature > 128)
	{
		int delta = s.temperature - 128;
		s.temperature -= delta;
		int m = min( delta, s.fuel );
		delta -= m; s.fuel -= m;
		m = min( delta, s.laser );
		delta -= m; s.laser -= m;
		m = min( delta, s.cooler );
		delta -= m; s.cooler -= m;
		s.cores -= delta;
	}
	if (s.cores <= 0)
		return { kill_ship(s), vel };

	return { s, vel };
}

Point befo[1024];
bool used[1024];

vector< Point > get_vel( const vector<Ship> & before, const vector<Ship> & current )
{
	for (let & s : before)
	{
		befo[s.id] = s.pos;
		used[s.id] = true;
	}
	vector< Point > res;
	for (let & s : current)
		if (used[s.id])
			res.push_back( { s.pos.x-befo[s.id].x, s.pos.y-befo[s.id].y } );
		else res.push_back( { 0, 0 } );
	for (let & s : before)
	{
		befo[s.id] = { 0, 0 };
		used[s.id] = false;
	}
	return res;
}

GameState predict_next_state( const GameState &state, const GameState &prev_state, const vector<Command> & commands )
{
	GameState new_state;
	new_state.planet_size = state.planet_size;
	new_state.role = state.role;
	new_state.ticks_left = state.ticks_left-1;
	Point pp = { state.planet_size.x/2, state.planet_size.y/2 };

	let my_vel = get_vel( prev_state.my_ships, state.my_ships );
	let enemy_vel = get_vel( prev_state.enemy_ships, state.enemy_ships );

	int n = (int)state.my_ships.size();
	for (int i=0; i<n; i++)
	{
		let s = state.my_ships[i];
		let & c = commands[i];
		let s_v = predict_ship_state( s, my_vel[i], pp, c );
		if (s_v.s.cores > 0)
			new_state.my_ships.push_back( s_v.s );
	}
	int m = (int)state.enemy_ships.size();
	for (int i=0; i<m; i++)
	{
		let s = state.enemy_ships[i];
		let s_v = predict_ship_state( s, enemy_vel[i], pp, Command() );
		if (s_v.s.cores > 0)
			new_state.enemy_ships.push_back( s_v.s );
	}

	return new_state;
}

vector<Command> bot_func_icarus(const GameState &old_state, const GameState &state)
{
	vector<Command> cmd;
	Point pp = { state.planet_size.x/2, state.planet_size.y/2 };
	let my_vel = get_vel( old_state.my_ships, state.my_ships );
	//let enemy_vel = get_vel( old_state.enemy_ships, state.enemy_ships );

	int n = (int)state.my_ships.size();
	for (int i=0; i<n; i++)
	{
		Point best_acc = {0,0};
		int best_value = -1;
		for (int dx=-2; dx<=2; dx++)
			for (int dy=-2; dy<=2; dy++)
			{
				int fu = max( abs(dx), abs(dy) );
				if (fu <= state.my_ships[i].fuel)
				{
					MyShip ms = { state.my_ships[i], my_vel[i] };
					int steps = 0;
					Command cc;
					cc.acceleration = {dx, dy};
					ms = predict_ship_state( ms.s, ms.vel, pp, cc );
					if (ms.s.cores > 0) steps++;
					for (int j=1; j<min(100,state.ticks_left); j++)
					{
						ms = predict_ship_state( ms.s, ms.vel, pp, Command() );
						if (ms.s.cores > 0) steps++;
						else break;
					}
					int cur_value = (2-fu)*10 + steps;
					//if (steps < 10) cur_value = steps;
					if (cur_value > best_value)
					{
						best_value = cur_value;
						best_acc = { dx, dy };
					}
				}
			}
		Command ccc;
		int diag = abs( abs( state.my_ships[i].pos.x ) - abs( state.my_ships[i].pos.y ) );
		int velo = abs( abs( my_vel[i].x ) + abs( my_vel[i].y ) );
		if (diag <= 10 && velo <= 4)
		{
			if ( (state.my_ships[i].pos.x<0 && state.my_ships[i].pos.y<0) ||
				(state.my_ships[i].pos.x>0 && state.my_ships[i].pos.y>0) )
				best_acc = { min(2, state.my_ships[i].fuel), min(2, state.my_ships[i].fuel) };
			else best_acc = { min(2, state.my_ships[i].fuel), -min(2, state.my_ships[i].fuel) };
		}
		ccc.acceleration = best_acc;
		cmd.push_back( ccc );
	}

	return cmd;
}

double power_of_angle[] = { 1., 0.1, 0., 0.05, 0.25, 0.05, 0., 0.1, 0.5, 0.1, 0., 0.05, 0.25, 0.05, 0., 0., 0., 0., 0., 0.05, 0.25, 0.05,
0., 0.1, 0.5, 0.1, 0., 0.05, 0.25, 0.05, 0., 0.1, 1. };

vector<Command> bot_func_sniper(const GameState &old_state, const GameState &state)
{
	vector<Command> cmd;
	Point pp = { state.planet_size.x/2, state.planet_size.y/2 };
	let my_vel = get_vel( old_state.my_ships, state.my_ships );
	let enemy_vel = get_vel( old_state.enemy_ships, state.enemy_ships );

	int n = (int)state.my_ships.size();
	int m = (int)state.enemy_ships.size();
	for (int i=0; i<n; i++)
	{
		int best_enemy = -1;
		double best_value = 0.;
		MyShip ms = predict_ship_state( state.my_ships[i], my_vel[i], pp, Command() );
		Point m_pos = ms.s.pos;
		for (int j=0; j<m; j++)
		{
			MyShip es = predict_ship_state( state.enemy_ships[j], enemy_vel[j], pp, Command() );
			Point vec = { es.s.pos.x - m_pos.x, es.s.pos.y - m_pos.y };
			Point vec2 = { abs(vec.x), abs(vec.y) };
			if (vec2.x + vec2.y < 2 * state.my_ships[i].laser)
			{
				if (vec2.x < vec2.y) swap( vec2.x, vec2.y );
				if (vec2.x > 0)
				{
					double ratio = (double)vec2.y / (double)vec2.x;
					int ind = (int)(ratio * 32 + 0.5);
					if (ind < 0) ind = 0;
					if (ind > 32) ind = 8;
					double pw = power_of_angle[ind];
					double L = (double)state.my_ships[i].laser;
					double C = (double)state.my_ships[i].cooler;
					double T = (double)state.my_ships[i].temperature;
					double value = pw * L;
					value -= max(0., (double)(T + L - C) - 64.);
					value += ((double)state.enemy_ships[j].laser / get_square( state.enemy_ships[j] )) * 2.;
					if (value > best_value)
					{
						best_value = value;
						best_enemy = j;
					}
				}
			}
		}
		Command ccc;
		if (state.my_ships[i].laser > 0)
			if (best_enemy > -1)
			{
				ccc.shoot = true;
				MyShip es = predict_ship_state( state.enemy_ships[best_enemy], enemy_vel[best_enemy], pp, Command() );
				ccc.shoot_target = es.s.pos;
			}
		cmd.push_back( ccc );
	}

	return cmd;
}

vector<Command> bot_func_kamikaze(const GameState &old_state, const GameState &state)
{
	vector<Command> cmd;
	Point pp = { state.planet_size.x/2, state.planet_size.y/2 };
	let my_vel = get_vel( old_state.my_ships, state.my_ships );
	let enemy_vel = get_vel( old_state.enemy_ships, state.enemy_ships );

	int n = (int)state.my_ships.size();
	int m = (int)state.enemy_ships.size();
	for (int i=0; i<n; i++)
	{
		Command ccc;
		// boom?
		Point best_acc = {0,0};
		int best_dist = 10;
		for (int dx=-2; dx<=2; dx++)
			for (int dy=-2; dy<=2; dy++)
			{
				int fu = max( abs(dx), abs(dy) );
				if (fu <= state.my_ships[i].fuel)
				{
					for (int target=0; target<m; target++)
					{
						Command cc;
						cc.acceleration = {dx, dy};
						// int steps = 0;
						MyShip ms = { state.my_ships[i], my_vel[i] };
						MyShip es = { state.enemy_ships[target], enemy_vel[target] };
						ms = predict_ship_state( ms.s, ms.vel, pp, cc );
						es = predict_ship_state( es.s, es.vel, pp, Command() );
						if (ms.s.cores > 0)
						{
							Point vec = { es.s.pos.x - ms.s.pos.x, es.s.pos.y - ms.s.pos.y };
							int dist = (abs(vec.x) + abs(vec.y));
							if (dist < best_dist)
							{
								best_dist = dist;
								best_acc = { dx, dy };
							}
						}
					}
				}
			}
		if (best_dist <= 2) // boom!
		{
			ccc.explode = true;
			ccc.acceleration = best_acc;
			cmd.push_back( ccc );
			continue;
		}
		// find a target
		int best_value = 1000000;
		for (int dx=-2; dx<=2; dx++)
			for (int dy=-2; dy<=2; dy++)
			{
				int fu = max( abs(dx), abs(dy) );
				if (fu <= state.my_ships[i].fuel)
				{
					for (int target=0; target<m; target++)
					{
						Command cc;
						cc.acceleration = {dx, dy};
						int steps = 0;
						MyShip ms = { state.my_ships[i], my_vel[i] };
						MyShip es = { state.enemy_ships[target], enemy_vel[target] };
						for (int j=0; j<min(100,state.ticks_left); j++)
						{
							steps++;
							ms = predict_ship_state( ms.s, ms.vel, pp, j==0 ? cc : Command() );
							es = predict_ship_state( es.s, es.vel, pp, Command() );
							if (ms.s.cores > 0)
							{
								Point vec = { es.s.pos.x - ms.s.pos.x, es.s.pos.y - ms.s.pos.y };
								int value = fu*100 + steps*10 + (abs(vec.x) + abs(vec.y));
								if (value < best_value)
								{
									best_value = value;
									best_acc = { dx, dy };
								}
							}
							else break;
						}
					}
				}
			}
		ccc.acceleration = best_acc;
		cmd.push_back( ccc );
	}

	return cmd;
}

vector<Command> bot_func_kamikaze2(const GameState &old_state, const GameState &state)
{
	vector<Command> cmd;
	Point pp = { state.planet_size.x/2, state.planet_size.y/2 };
	let my_vel = get_vel( old_state.my_ships, state.my_ships );
	let enemy_vel = get_vel( old_state.enemy_ships, state.enemy_ships );

	int n = (int)state.my_ships.size();
	int m = (int)state.enemy_ships.size();
	for (int i=0; i<n; i++)
	{
		Command ccc;
		// boom?
		Point best_acc = {0,0};
		int best_dist = 10;
		for (int dx=-2; dx<=2; dx++)
			for (int dy=-2; dy<=2; dy++)
			{
				int fu = max( abs(dx), abs(dy) );
				if (fu <= state.my_ships[i].fuel)
				{
					for (int target=0; target<m; target++)
					{
						Command cc;
						cc.acceleration = {dx, dy};
						// int steps = 0;
						MyShip ms = { state.my_ships[i], my_vel[i] };
						MyShip es = { state.enemy_ships[target], enemy_vel[target] };
						ms = predict_ship_state( ms.s, ms.vel, pp, cc );
						es = predict_ship_state( es.s, es.vel, pp, Command() );
						if (ms.s.cores > 0)
						{
							Point vec = { es.s.pos.x - ms.s.pos.x, es.s.pos.y - ms.s.pos.y };
							int dist = (abs(vec.x) + abs(vec.y));
							if (dist < best_dist)
							{
								best_dist = dist;
								best_acc = { dx, dy };
							}
						}
					}
				}
			}
		if (best_dist <= 2) // boom!
		{
			ccc.explode = true;
			ccc.acceleration = best_acc;
		}
		cmd.push_back( ccc );
	}

	return cmd;
}

vector<Command> bot_func_combine(const GameState &old_state, const GameState &state)
{
	vector<Command> cmd;
	let mv = bot_func_icarus(old_state, state);
	let sh = bot_func_sniper(old_state, state);
	for (int i=0; i<(int)state.my_ships.size(); i++)
	{
		Command c;
		c.acceleration = mv[i].acceleration;
		c.shoot = sh[i].shoot;
		c.shoot_target = sh[i].shoot_target;
		cmd.push_back( c );
	}
	return cmd;
}

vector<Command> bot_func_assassin(const GameState &old_state, const GameState &state)
{
	vector<Command> cmd;
	let mv = bot_func_kamikaze(old_state, state);
	let sh = bot_func_sniper(old_state, state);
	for (int i=0; i<(int)state.my_ships.size(); i++)
	{
		Command c;
		c.acceleration = mv[i].acceleration;
		c.explode = mv[i].explode;
		c.shoot = sh[i].shoot;
		c.shoot_target = sh[i].shoot_target;
		cmd.push_back( c );
	}
	return cmd;
}

vector<Command> bot_func_combine2(const GameState &old_state, const GameState &state)
{
	if (state.role == Role::kAttacker)
		return bot_func_assassin( old_state, state );
	else return bot_func_combine( old_state, state );
}

vector<Command> bot_func_combine3(const GameState &old_state, const GameState &state)
{
	if (state.role == Role::kAttacker)
	{
		vector<Command> cmd;
		let ka = bot_func_kamikaze2(old_state, state);
		let ss = bot_func_combine(old_state, state);
		for (int i=0; i<(int)state.my_ships.size(); i++)
		{
			Command c;
			if (ka[i].explode)
			{
				c.acceleration = ka[i].acceleration;
				c.explode = ka[i].explode;
			}
			else
			{
				c.acceleration = ss[i].acceleration;
			}
			c.shoot = ss[i].shoot;
			c.shoot_target = ss[i].shoot_target;
			cmd.push_back( c );
		}
		return cmd;
	}
	else return bot_func_combine( old_state, state );
}

void print_state(const GameState &state)
{
	cerr << "role:" << (state.role == Role::kAttacker ? "Attacker" : "Defender") << " ";
	cerr << "ticks_left:" << state.ticks_left << " " << "planet_size:(" << state.planet_size.x << "," << state.planet_size.y << ")" << endl;
	cerr << "my_ships[" << state.my_ships.size() << "]:" << endl;
	for (let s : state.my_ships)
		cerr << " [pos:(" << s.pos.x << "," << s.pos.y << ") T:" << s.temperature <<
			" F:" << s.fuel << " L:" << s.laser << " C:" << s.cooler << " K:" << s.cores << "]" << endl;
	cerr << "enemy_ships[" << state.my_ships.size() << "]:" << endl;
	for (let s : state.enemy_ships)
		cerr << " [pos:(" << s.pos.x << "," << s.pos.y << ") T:" << s.temperature <<
			" F:" << s.fuel << " L:" << s.laser << " C:" << s.cooler << " K:" << s.cores << "]" << endl;
	cerr << endl;
}
