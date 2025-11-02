# Wind Mouse Relative Movement Simulation

This project implements a **realistic mouse movement generator** based on a "wind and gravity" model. (windmouse algorithm   based on https://ben.land/post/2021/04/25/windmouse-human-mouse-movement/)
It uses deterministic pseudo-randomness (`xorshift32`) instead of rand() for performance and compability with embedded and kmdf code

---

## ✨ Features

- **Fast pseudo-random generator** using `xorshift32`
- **Custom integer-based hypot approximation** for performance
- **Smooth, human-like motion** driven by simulated gravity and wind
- **No floating-point operations** — fully integer math
- **Deterministic randomness** (based on seed)
- **Adjustable parameters**:
  - `gravity_strength` — how strongly the movement is pulled toward the target
  - `max_wind_magnitude` — how chaotic the movement appears
  - `max_step_size` — the maximum pixel movement per step
  - `duration_microsecond` — total movement duration

---

## 🧩 Code

```cpp
#define scaleFactor 128

unsigned int seed = 123456789;
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

int wind_mouse_relative_move( //returns iterations count for example
	short delta_x, short delta_y,
	unsigned char gravity_strength = 10,
	unsigned char max_wind_magnitude = 1,
	unsigned char max_step_size = 32,
	unsigned int duration_microsecond = 1000 * 1000
)
{
	// gravity_strength      = Gravity constant       Pull toward goal
	// max_wind_magnitude    = Max wind magnitude     Controls random jitter
	// max_step_size         = Max velocity           Upper limit of speed, px per move, distance threshold
	// wind_decay_factor     = Normalization constant Keep energy stable
	// velocity_x, velocity_y = Velocity vector       Accumulated motion
	// wind_x, wind_y        = Wind vector            Random influence

	int iteration_count = 0;
	constexpr unsigned char wind_decay_factor = 2;

	short current_x = 0;
	short current_y = 0;
	short prev_x = 0;
	short prev_y = 0;

	int velocity_x = 0;
	int velocity_y = 0;
	int wind_x = 0;
	int wind_y = 0;

	unsigned short total_distance = fast_hypot(delta_x, delta_y);
	unsigned short distance_to_target = total_distance;

	while (true) {
		iteration_count++;

		if (distance_to_target > max_step_size) {
			// Apply wind (random jitter)
			int wind_magnitude = (max_wind_magnitude < distance_to_target)
				? max_wind_magnitude
				: distance_to_target;

			wind_x = wind_x / wind_decay_factor + fast_rand() * wind_magnitude;
			wind_y = wind_y / wind_decay_factor + fast_rand() * wind_magnitude;

			// Apply gravity (pull toward target) and wind
			velocity_x += wind_x + gravity_strength * scaleFactor * (delta_x - current_x) / distance_to_target;
			velocity_y += wind_y + gravity_strength * scaleFactor * (delta_y - current_y) / distance_to_target;

			// Cap velocity at maximum
			short velocity_magnitude = fast_hypot(velocity_x, velocity_y);
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
			int step_distance = fast_hypot(step_x, step_y);
			int sleep_duration = duration_microsecond * step_distance / distance_to_target;
			duration_microsecond -= sleep_duration;

			// Execute movement
			drawdot_mouse_move_relative(current_x - prev_x, current_y - prev_y);
			SleepMicroseconds(sleep_duration);
			prev_x = current_x;
			prev_y = current_y;

			// New distance to target
			distance_to_target = fast_hypot(delta_x - current_x, delta_y - current_y);
		}
		else {
			// Final movement directly to target
			drawdot_mouse_move_relative(delta_x - prev_x, delta_y - prev_y);
			SleepMicroseconds(duration_microsecond);
			break;
		}


	}

	return iteration_count;
}
