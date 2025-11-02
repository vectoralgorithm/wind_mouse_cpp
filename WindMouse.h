#pragma once
#define WindMouseDebug

constexpr uint32_t fnv1a_32(const char* str, uint32_t hash = 2166136261u) {
	return *str ? fnv1a_32(str + 1, (hash ^ static_cast<uint32_t>(*str)) * 16777619u) : hash;
}
constexpr uint32_t compile_time_seed() {
	return fnv1a_32(__DATE__) ^ fnv1a_32(__TIME__);
}


template<typename MoveCallback, typename SleepCallback>
void interpolateMouseMovements(
	short deltaX,
	short deltaY,
	unsigned int duration,
	MoveCallback moveCallback,
	SleepCallback sleepCallback
) {
	if (deltaX == 0 && deltaY == 0) {
		sleepCallback(duration);
		return;
	}

	unsigned short absDeltaX = (deltaX < 0) ? -deltaX : deltaX;
	unsigned short absDeltaY = (deltaY < 0) ? -deltaY : deltaY;

	char signX = (deltaX > 0) ? 1 : -1;
	char signY = (deltaY > 0) ? 1 : -1;

	unsigned short maxSteps = (absDeltaX > absDeltaY) ? absDeltaX : absDeltaY;

	if (maxSteps > 0) {
		duration = duration / maxSteps;
	}

	short error = 0;
	if (absDeltaX > absDeltaY) {
		// X is dominant axis
		for (int i = 0; i < absDeltaX; i++) {
			error += absDeltaY;
			char stepY = 0;
			if (2 * error >= absDeltaX) {
				stepY = signY;
				error -= absDeltaX;
			}
			moveCallback(signX, stepY);
			sleepCallback(duration);
		}
	}
	else {
		// Y is dominant axis
		for (int i = 0; i < absDeltaY; i++) {
			error += absDeltaX;
			char stepX = 0;
			if (2 * error >= absDeltaY) {
				stepX = signX;
				error -= absDeltaY;
			}
			moveCallback(stepX, signY);
			sleepCallback(duration);
		}
	}
}


constexpr unsigned char scaleFactor = 128;

unsigned int seed = compile_time_seed();
unsigned int xorshift32() {
	unsigned int x = seed;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return seed = x;
}
unsigned char fast_rand_unsigned() {
	//0 to 255
	return xorshift32() & (scaleFactor * 2 - 1);
}
char fast_rand() {
	// -128 127
	return fast_rand_unsigned() - scaleFactor;
}

template<typename T>
auto fast_hypot(T x, T y) {
	auto dx = (x < 0) ? -x : x;
	auto dy = (y < 0) ? -y : y;
	auto max_val = (dx > dy) ? dx : dy;
	auto min_val = (dx > dy) ? dy : dx;
	return (15 * max_val + 7 * min_val) >> 4;
}

template<typename MoveCallback, typename SleepCallback>
#ifdef WindMouseDebug
int
#else
void
#endif
wind_mouse_relative_move( //returns iterations count for example
	short delta_x, short delta_y,
	MoveCallback moveCallback,
	SleepCallback sleepCallback,
	unsigned int duration_microsecond_remained = 1000 * 1000,
	unsigned char gravity_strength = 10,
	unsigned char max_wind_magnitude = 2,
	unsigned char max_step_size = 32
)
{
	// gravity_strength      = Gravity constant       Pull toward goal
	// max_wind_magnitude    = Max wind magnitude     Controls random jitter
	// max_step_size         = Max velocity           Upper limit of speed, px per move, distance threshold
	// wind_decay_factor     = Normalization constant Keep energy stable
	// velocity_x, velocity_y = Velocity vector       Accumulated motion
	// wind_x, wind_y        = Wind vector            Random influence

#ifdef WindMouseDebug
	int iteration_count = 0;
#endif

	constexpr unsigned char wind_decay_factor = 2;

	short current_x = 0;
	short current_y = 0;
	short prev_x = 0;
	short prev_y = 0;

	int velocity_x = 0;
	int velocity_y = 0;
	short wind_x = 0;
	short wind_y = 0;

	unsigned short total_distance = fast_hypot(delta_x, delta_y);
	unsigned short distance_to_target = total_distance;

	while (true) {

#ifdef WindMouseDebug
		iteration_count++;
#endif

		if (distance_to_target > max_step_size) {
			// Apply wind (random jitter)
			unsigned short wind_magnitude = (max_wind_magnitude < distance_to_target)
				? max_wind_magnitude
				: distance_to_target;

			wind_x = wind_x / wind_decay_factor + fast_rand() * wind_magnitude;
			wind_y = wind_y / wind_decay_factor + fast_rand() * wind_magnitude;

			// Apply gravity (pull toward target) and wind
			velocity_x += wind_x + gravity_strength * scaleFactor * (delta_x - current_x) / distance_to_target;
			velocity_y += wind_y + gravity_strength * scaleFactor * (delta_y - current_y) / distance_to_target;

			// Cap velocity at maximum
			unsigned short velocity_magnitude = fast_hypot(velocity_x, velocity_y);
			if (velocity_magnitude > max_step_size * scaleFactor) {
				velocity_x = velocity_x / velocity_magnitude * max_step_size;
				velocity_y = velocity_y / velocity_magnitude * max_step_size;
			}

			// Calculate movement for this step
			short step_x = velocity_x / scaleFactor;
			short step_y = velocity_y / scaleFactor;
			current_x += step_x;
			current_y += step_y;

			// Calculate timing for this step
			unsigned short step_distance = fast_hypot(step_x, step_y);
			unsigned int sleep_duration = duration_microsecond_remained * step_distance / distance_to_target;
			duration_microsecond_remained -= sleep_duration;

			// Execute movement
			//callback(current_x - prev_x, current_y - prev_y, sleep_duration);
			interpolateMouseMovements(current_x - prev_x, current_y - prev_y, sleep_duration, moveCallback, sleepCallback);
			prev_x = current_x;
			prev_y = current_y;

			// New distance to target
			distance_to_target = fast_hypot(delta_x - current_x, delta_y - current_y);
		}
		else {
			// Final movement directly to target
			interpolateMouseMovements(delta_x - prev_x, delta_y - prev_y, duration_microsecond_remained, moveCallback, sleepCallback);
			break;
		}


	}
#ifdef WindMouseDebug
	return iteration_count;
#endif
}