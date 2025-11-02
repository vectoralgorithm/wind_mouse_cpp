# 🌀 Wind Mouse Relative Movement Simulation

<p align="center">
  <img src="https://raw.githubusercontent.com/vectoralgorithm/wind_mouse_cpp/main/Untitled.png" alt="Wind Mouse Simulation Demo" width="600">
</p>

A lightweight, **deterministic, human-like mouse movement generator** based on the ["WindMouse" algorithm](https://ben.land/post/2021/04/25/windmouse-human-mouse-movement/).

This implementation avoids floating-point operations entirely — making it suitable for **embedded systems, kernel-mode (KMDF)**, and **high-performance applications**.  
It uses a custom integer math model driven by simulated *gravity* and *wind forces* to create smooth, natural-looking motion.

---

## ✨ Features

- 🧩 **Single header-only implementation**
- ⚡ **Fast pseudo-random generator** using `xorshift32`
- 🌱 **Random seed generated at build time**
- 🧮 **No floating-point math** — fully integer-based calculations
- 🎯 **Smooth, human-like motion** via simulated gravity & wind
- 🧠 **Deterministic randomness** (perfect reproducibility)
- ⚙️ **Adjustable parameters:**
  - `gravity_strength` — how strongly movement is pulled toward the target
  - `max_wind_magnitude` — randomness intensity
  - `max_step_size` — maximum pixel distance per step
  - `duration_microsecond` — total movement duration

---

## 🧠 Algorithm Overview

The motion is computed step-by-step, influenced by:
- **Gravity**: pulls the cursor smoothly toward the target  
- **Wind**: introduces randomized, decaying offsets to simulate human jitter  
- **Velocity normalization**: caps movement speed for smooth acceleration/deceleration

Each iteration updates:
- Wind vector  
- Velocity vector  
- Step distance & timing  

and executes the movement with a delay (`SleepMicroseconds`) proportional to the step size.

---

## 🧩 Minimal Example

*(See the full updated version in [`WindMouse.h`](./WindMouse.h))*

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
	return xorshift32() & (scaleFactor * 2 - 1);
}
char fast_rand() {
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

int wind_mouse_relative_move(
	short delta_x, short delta_y,
	unsigned char gravity_strength = 10,
	unsigned char max_wind_magnitude = 2,
	unsigned char max_step_size = 320,
	unsigned int duration_microsecond_remained = 1000 * 1000
) {
	int iteration_count = 0;
	constexpr unsigned char wind_decay_factor = 2;

	short current_x = 0, current_y = 0, prev_x = 0, prev_y = 0;
	int velocity_x = 0, velocity_y = 0;
	short wind_x = 0, wind_y = 0;

	unsigned short total_distance = fast_hypot(delta_x, delta_y);
	unsigned short distance_to_target = total_distance;

	while (true) {
		iteration_count++;

		if (distance_to_target > max_step_size) {
			unsigned short wind_magnitude = (max_wind_magnitude < distance_to_target)
				? max_wind_magnitude
				: distance_to_target;

			wind_x = wind_x / wind_decay_factor + fast_rand() * wind_magnitude;
			wind_y = wind_y / wind_decay_factor + fast_rand() * wind_magnitude;

			velocity_x += wind_x + gravity_strength * scaleFactor * (delta_x - current_x) / distance_to_target;
			velocity_y += wind_y + gravity_strength * scaleFactor * (delta_y - current_y) / distance_to_target;

			unsigned short velocity_magnitude = fast_hypot(velocity_x, velocity_y);
			if (velocity_magnitude > max_step_size * scaleFactor) {
				velocity_x = velocity_x / velocity_magnitude * max_step_size;
				velocity_y = velocity_y / velocity_magnitude * max_step_size;
			}

			short step_x = velocity_x / scaleFactor;
			short step_y = velocity_y / scaleFactor;
			current_x += step_x;
			current_y += step_y;

			unsigned short step_distance = fast_hypot(step_x, step_y);
			unsigned int sleep_duration = duration_microsecond_remained * step_distance / distance_to_target;
			duration_microsecond_remained -= sleep_duration;

			drawdot_mouse_move_relative(current_x - prev_x, current_y - prev_y);
			SleepMicroseconds(sleep_duration);
			prev_x = current_x;
			prev_y = current_y;

			distance_to_target = fast_hypot(delta_x - current_x, delta_y - current_y);
		}
		else {
			drawdot_mouse_move_relative(delta_x - prev_x, delta_y - prev_y);
			SleepMicroseconds(duration_microsecond_remained);
			break;
		}
	}
	return iteration_count;
}
