#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <type_traits>
#include <chrono>
#include <thread>
#include "WindMouse.h"

HDC g_hdc = nullptr;
int g_mouseX = 0;
int g_mouseY = 0;

//Percise
void SleepMicroseconds(int64_t microseconds) {
	LARGE_INTEGER freq, start, now;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	int64_t target = start.QuadPart + (microseconds * freq.QuadPart) / 1'000'000LL;
	do { QueryPerformanceCounter(&now); } while (now.QuadPart < target);
}


void DrawDotRelative(int dx, int dy) {
	if (!g_hdc) return;

	g_mouseX = max(0, min(g_mouseX + dx, 799));
	g_mouseY = max(0, min(g_mouseY + dy, 599));

	COLORREF red = RGB(255, 0, 0);
	for (int x = 0; x < 2; ++x)
		for (int y = 0; y < 2; ++y)
			SetPixel(g_hdc, g_mouseX + x, g_mouseY + y, red);
}


template <typename Func>
auto MeasureTime(Func func) -> decltype(func()) {
	LARGE_INTEGER freq, start, end;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);

	if constexpr (std::is_void_v<decltype(func())>) {
		func();
		QueryPerformanceCounter(&end);
		double elapsed_ms = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
		printf("Execution time: %.3f ms\n", elapsed_ms);
	}
	else {
		auto result = func();
		QueryPerformanceCounter(&end);
		double elapsed_ms = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
		printf("Execution time: %.3f ms, WindIterations: %d\n", elapsed_ms, result);
		return result;
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		g_hdc = GetDC(hwnd);
		break;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);



		auto moveCallback = [](short dx, short dy) {
			DrawDotRelative(dx, dy);
			};

		auto sleepCallbackPercise = [](unsigned int sleep_us) {
			SleepMicroseconds(sleep_us);
			};
		auto sleepCallbackChrono = [](unsigned int sleep_us) {
			std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
			};
		auto sleepCallbackImperfect = [](unsigned int sleep_us) {
			Sleep(sleep_us / 1000);
			};

		auto getCurrentTimeChrono = []() -> unsigned int {
			return std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now().time_since_epoch()
			).count();
			};
		auto getCurrentTimeImpefect = []() -> unsigned int {
			// GetTickCount64() = milliseconds
			return GetTickCount64() * 1000; //microseconds
			};


		//// Interpolation with perfect sleep
		//for (int i = 0; i < 10; ++i) {
		//	g_mouseX = 0;
		//	g_mouseY = 50 + i * 50;
		//	MeasureTime([&]() {
		//		return interpolateMouseMovePerfect(800, 0, 1000 * 1000, moveCallback, sleepCallbackPercise);
		//		});
		//}

		//// Interpolation with Chrono sleep
		//for (int i = 0; i < 10; ++i) {
		//	g_mouseX = 0;
		//	g_mouseY = 50 + i * 50;
		//	MeasureTime([&]() {
		//		return interpolateMouseMoveImperfect(800, 0, 1000 * 1000, moveCallback, sleepCallbackChrono, getCurrentTimeChrono);
		//		});
		//}

		//// Interpolation with imperfect sleep
		//for (int i = 0; i < 10; ++i) {
		//	g_mouseX = 0;
		//	g_mouseY = 50 + i * 50;
		//	MeasureTime([&]() {
		//		return interpolateMouseMoveImperfect(800, 0, 1000 * 1000, moveCallback, sleepCallbackImperfect, getCurrentTimeImpefect);
		//		});
		//}


		//WindMouse and Linear Interpolation with perfect sleep
		//Interpolates with delta of 1, extremely smooth, requires extremely percise sleep
		for (int i = 0; i < 10; ++i) {
			g_mouseX = 0;
			g_mouseY = 50 + i * 50;
			MeasureTime([&]() {
				return wind_mouse_perfect(800, 0, 1000 * 1000, moveCallback, sleepCallbackPercise);
				});
		}

		////WindMouse and Linear Interpolation with chrono sleep
		////!!!If sleep is imperfect it means we can't interpolate with 1 delta step, we need greater step to mach percision of sleep and timer
		//for (int i = 0; i < 10; ++i) {
		//	g_mouseX = 0;
		//	g_mouseY = 50 + i * 50;
		//	MeasureTime([&]() {
		//		return wind_mouse_imperfect(800, 0, 1000 * 1000, moveCallback, sleepCallbackChrono, getCurrentTimeChrono, 10, 2, 32);
		//		});
		//}


		////!!!WORST!!! MOST IMPERFECT SLEEP AND TIMER SCENARIO - comment this and uncomment better scenarios
		////!!!If sleep is imperfect it means we can't interpolate with 1 delta step, we need greater step to mach percision of sleep and timer
		////WindMouse and Linear Interpolation with imperfect sleep
		//for (int i = 0; i < 10; ++i) {
		//	g_mouseX = 0;
		//	g_mouseY = 50 + i * 50;
		//	MeasureTime([&]() {
		//		return wind_mouse_imperfect(800, 0, 1000 * 1000, moveCallback, sleepCallbackImperfect, getCurrentTimeImpefect, 10, 2, 32);
		//		});
		//}


		

		EndPaint(hwnd, &ps);
		break;
	}

	case WM_DESTROY:
		if (g_hdc) ReleaseDC(hwnd, g_hdc);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);

	WNDCLASSW wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"DrawApp";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	RegisterClassW(&wc);

	RECT rect = { 0, 0, 800, 600 };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hwnd = CreateWindowExW(
		0, L"DrawApp", L"Wind Mouse Demo", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, hInstance, nullptr
	);

	ShowWindow(hwnd, nCmdShow);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
