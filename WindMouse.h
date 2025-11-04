#pragma once

//#define WindMouseDebug


// Constexpr helpers to detect if arg is empty and build separate versions of a function
template<typename T, typename U>
struct IS_SAME_TYPE { static constexpr bool value = false; };

template<typename T>
struct IS_SAME_TYPE<T, T> { static constexpr bool value = true; };

template<typename T, typename U>
inline constexpr bool IS_SAME_TYPE_v = IS_SAME_TYPE<T, U>::value;

struct NO_CALLBACK {};


// BuildTime random seed generation
constexpr unsigned int fnv1a_32(const char* str, unsigned int hash = 2166136261u) {
	return *str ? fnv1a_32(str + 1, (hash ^ static_cast<unsigned int>(*str)) * 16777619u) : hash;
}
constexpr unsigned int compile_time_seed() {
	return fnv1a_32(__DATE__) ^ fnv1a_32(__TIME__);
}


/**
 * @brief Interpolates movement, guarantees deltax, deltay final point and duration, requires percise sleep
 *
 * @tparam MoveCallback Callable for executing mouse movement: void(short dx, short dy)
 * @tparam SleepCallback Callable for delays: void(unsigned int microseconds)
 *
 * @param deltaX Total horizontal distance to move (pixels, can be negative)
 * @param deltaY Total vertical distance to move (pixels, can be negative)
 * @param duration_us Total duration for movement (microseconds)
 * @param moveDelta Function to execute incremental movement
 * @param sleepPerfect Function to sleep for specified microseconds
 *
 * @note sleep must be percise
 */
template<typename MoveCallback, typename SleepCallback>
void interpolateMouseMovePerfect(
	short deltaX,
	short deltaY,
	unsigned int duration_us,
	MoveCallback moveDelta,
	SleepCallback sleepPerfect
) {
	if (deltaX == 0 && deltaY == 0) {
		sleepPerfect(duration_us);
		return;
	}

	int signX = (deltaX >= 0) ? 1 : -1;
	int signY = (deltaY >= 0) ? 1 : -1;
	int absX = (deltaX >= 0) ? deltaX : -deltaX;
	int absY = (deltaY >= 0) ? deltaY : -deltaY;

	int steps = (absX >= absY) ? absX : absY;
	if (steps == 0) steps = 1; // avoid divide-by-zero

	unsigned int stepTime = duration_us / static_cast<unsigned int>(steps);

	int accX = 0;
	int accY = 0;
	int currX = 0;
	int currY = 0;

	for (int i = 0; i < steps; ++i) {
		accX += absX;
		accY += absY;

		int moveX = 0;
		int moveY = 0;

		if (accX >= steps) {
			accX -= steps;
			moveX = signX;
		}
		if (accY >= steps) {
			accY -= steps;
			moveY = signY;
		}

		currX += moveX;
		currY += moveY;

		moveDelta(moveX, moveY);
		sleepPerfect(stepTime);
	}

	// Final correction for rounding
	int finalX = absX - ((currX >= 0) ? currX : -currX);
	int finalY = absY - ((currY >= 0) ? currY : -currY);

	if (finalX != 0 || finalY != 0) {
		moveDelta(finalX * signX, finalY * signY);
	}
}

/**
 * @brief Interpolates movement, guarantees deltax, deltay final point and duration 
 *
 * @tparam MoveCallback Callable for executing mouse movement: void(short dx, short dy)
 * @tparam SleepCallback Callable for delays: void(unsigned int microseconds)
 * @tparam GetTimeCallback Callable returning current time: unsigned long long() in microseconds
 *
 * @param deltaX Total horizontal distance to move (pixels, can be negative)
 * @param deltaY Total vertical distance to move (pixels, can be negative)
 * @param duration_us Total duration for movement (microseconds)
 * @param moveDelta Function to execute incremental mouse movement (dx, dy)
 * @param sleepImperfect Function to sleep for specified microseconds (may be imprecise)
 * @param getTime_us Function to get current timestamp in microseconds
 *
 * @note If sleep is not percise: make a bigger step to be where expected
 */
template<typename MoveCallback, typename SleepCallback, typename GetTimeCallback>
void interpolateMouseMoveImperfect(
	short deltaX,
	short deltaY,
	unsigned int duration_us,
	MoveCallback moveDelta,
	SleepCallback sleepImperfect,
	GetTimeCallback getTime_us
) {
	if (deltaX == 0 && deltaY == 0) {
		sleepImperfect(duration_us);
		return;
	}

	unsigned long long start = getTime_us();
	unsigned long long end = start + duration_us;

	int absX = (deltaX >= 0) ? deltaX : -deltaX;
	int absY = (deltaY >= 0) ? deltaY : -deltaY;
	int signX = (deltaX >= 0) ? 1 : -1;
	int signY = (deltaY >= 0) ? 1 : -1;

	unsigned int timePerPixel = (absX > absY)
		? (duration_us / absX)
		: (duration_us / absY);

	int currX = 0, currY = 0;

	while (true) {
		unsigned long long now = getTime_us();
		if (now >= end) break;

		unsigned int elapsed = now - start;

		// Expected position
		int targetX = (absX * elapsed) / duration_us;
		int targetY = (absY * elapsed) / duration_us;

		int dx = targetX - currX;
		int dy = targetY - currY;

		if (dx != 0 || dy != 0) {
			moveDelta(static_cast<short>(dx * signX), static_cast<short>(dy * signY));
			currX += dx;
			currY += dy;
		}

		unsigned int remaining = end - now;
		unsigned int sleepTime = (timePerPixel < remaining) ? timePerPixel : remaining;
		sleepImperfect(sleepTime);
	}

	// Final correction for any rounding errors
	int finalX = absX - currX;
	int finalY = absY - currY;
	if (finalX != 0 || finalY != 0) {
		moveDelta(static_cast<short>(finalX * signX), static_cast<short>(finalY * signY));
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

/**
	* @brief WindMouse with guaranteed: deltaX, deltaY final point reched + guaranteed duration for perfect sleep
	*
	* @tparam MoveCallback Callable for executing mouse movement (dx, dy)
	* @tparam SleepCallback Callable for delays (microseconds)
	*
	* @param delta_x Horizontal distance to move
	* @param delta_y Vertical distance to move
	* @param duration_us Total duration for movement (microseconds)
	* @param moveDelta Function to execute actual mouse movement
	* @param sleepPerfect Function to sleep/delay execution
	* @param gravity_strength Pull strength toward target
	* @param max_wind_magnitude Maximum random jitter magnitude
	* @param max_step_size Maximum velocity per step in
	*
	* @return iteration_count: Optional, #define WindMouseDebug required, void default
 */
template<typename MoveCallback, typename SleepCallback>
#ifdef WindMouseDebug
int  // returns iterations count for debug
#else
void
#endif
wind_mouse_perfect(
	short delta_x, short delta_y,
	unsigned int duration_remaining_us,
	MoveCallback moveDelta,
	SleepCallback sleepPerfect,
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

	unsigned short total_distance = static_cast<unsigned short>(fast_hypot(delta_x, delta_y));
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
			unsigned short velocity_magnitude = static_cast<unsigned short>(fast_hypot(velocity_x, velocity_y));
			if (velocity_magnitude > max_step_size * scaleFactor) {
				velocity_x = velocity_x / velocity_magnitude * max_step_size;
				velocity_y = velocity_y / velocity_magnitude * max_step_size;
			}

			// Calculate movement for this step
			short step_x = static_cast<short>(velocity_x / scaleFactor);
			short step_y = static_cast<short>(velocity_y / scaleFactor);
			current_x += step_x;
			current_y += step_y;

			// Calculate timing for this step
			unsigned short step_distance = static_cast<unsigned short>(fast_hypot(step_x, step_y));
			unsigned int sleep_duration = (distance_to_target > 0)
				? (duration_remaining_us * step_distance) / distance_to_target
				: duration_remaining_us;
			duration_remaining_us -= sleep_duration;

			// Execute movement
			interpolateMouseMovePerfect(current_x - prev_x, current_y - prev_y, sleep_duration, moveDelta, sleepPerfect);

			prev_x = current_x;
			prev_y = current_y;

			// New distance to target
			distance_to_target = static_cast<unsigned short>(fast_hypot(delta_x - current_x, delta_y - current_y));
		}
		else {
			// Final movement directly to target
			interpolateMouseMovePerfect(delta_x - prev_x, delta_y - prev_y, duration_remaining_us, moveDelta, sleepPerfect);

			break;
		}
	}
#ifdef WindMouseDebug
	return iteration_count;
#endif
}



/**
	* @brief WindMouse with guaranteed: deltaX, deltaY final point reched + guaranteed duration for imperfect sleep
	* 
	* @tparam MoveCallback Callable for executing mouse movement (dx, dy)
	* @tparam SleepCallback Callable for delays (microseconds)
	* @tparam GetTimeCallback Callable returning current time in microseconds
	* 
	* @param delta_x Horizontal distance to move
	* @param delta_y Vertical distance to move
	* @param duration_us Total duration for movement (microseconds)
	* @param moveDelta Function to execute actual mouse movement
	* @param sleepImperfect Function to sleep/delay execution
	* @param getTime_us Function to get current timestamp
	* @param gravity_strength Pull strength toward target
	* @param max_wind_magnitude Maximum random jitter magnitude
	* @param max_step_size Maximum velocity per step in pixels
	* 
	* @return iteration_count: Optional, #define WindMouseDebug required, void default
 */
template<typename MoveCallback, typename SleepCallback, typename GetTimeCallback>
#ifdef WindMouseDebug
int  // returns iterations count for debug
#else
void
#endif
wind_mouse_imperfect(
	short delta_x, short delta_y,
	unsigned int duration_us,
	MoveCallback moveDelta,
	SleepCallback sleepImperfect,
	GetTimeCallback getTime_us,
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
	unsigned short total_distance = static_cast<unsigned short>(fast_hypot(delta_x, delta_y));
	unsigned short distance_to_target = total_distance;

	// Timing improvements: track total duration and accumulated error
	unsigned int duration_remaining_us = duration_us;
	unsigned long long start_time = getTime_us();
	int accumulated_duration_error_us = 0;

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
			unsigned short velocity_magnitude = static_cast<unsigned short>(fast_hypot(velocity_x, velocity_y));
			if (velocity_magnitude > max_step_size * scaleFactor) {
				velocity_x = velocity_x / velocity_magnitude * max_step_size;
				velocity_y = velocity_y / velocity_magnitude * max_step_size;
			}
			// Calculate movement for this step
			short step_x = static_cast<short>(velocity_x / scaleFactor);
			short step_y = static_cast<short>(velocity_y / scaleFactor);
			current_x += step_x;
			current_y += step_y;
			// Calculate timing for this step
			unsigned short step_distance = static_cast<unsigned short>(fast_hypot(step_x, step_y));
			unsigned int ideal_sleep = (distance_to_target > 0)
				? (duration_remaining_us * step_distance) / distance_to_target
				: duration_remaining_us;

			// Compensate for accumulated timing error
			int compensated_sleep = static_cast<int>(ideal_sleep) - accumulated_duration_error_us;
			if (compensated_sleep < 0) compensated_sleep = 0;

			unsigned long long time_before = getTime_us();

			// Execute movement
			interpolateMouseMoveImperfect(current_x - prev_x, current_y - prev_y,
				static_cast<unsigned int>(compensated_sleep),
				moveDelta, sleepImperfect, getTime_us);

			unsigned long long time_after = getTime_us();
			unsigned int actual_elapsed = time_after - time_before;

			accumulated_duration_error_us += actual_elapsed - ideal_sleep;

			// Update remaining time based on actual wall-clock time
			unsigned int total_elapsed = time_after - start_time;
			duration_remaining_us = (total_elapsed < duration_us)
				? (duration_us - total_elapsed)
				: 0;

			prev_x = current_x;
			prev_y = current_y;

			// New distance to target
			distance_to_target = static_cast<unsigned short>(fast_hypot(delta_x - current_x, delta_y - current_y));
		}
		else {
			interpolateMouseMoveImperfect(delta_x - prev_x, delta_y - prev_y, duration_remaining_us, moveDelta, sleepImperfect, getTime_us);
			break;
		}
	}
#ifdef WindMouseDebug
	return iteration_count;
#endif
}
