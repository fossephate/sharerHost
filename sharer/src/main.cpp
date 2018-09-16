
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */

#include <stdio.h>
#include <fstream>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

#include "screen.h"
#include "base64.h"
#include "MouseController.hpp"
#include "KeyboardController.hpp"



// vigem:
#include "ViGEm/ViGEmClient.h"
#include "controllerManager.hpp"
#include "vigemController.hpp"

#include <gdiplus.h>
#pragma comment(lib, "Gdiplus.lib")




namespace fs = std::experimental::filesystem;

#include "tools.h"// includes windows.h

#if defined(_WIN32)
	#include <Windows.h>
	#include <Lmcons.h>
	#include <shlobj.h>
#endif

#include "sio/sio_client.h"


std::mutex _lock;


int main(int argc, char *argv[]) {

	// hide console window
	HWND hWnd = GetConsoleWindow();

	// Initialize GDI+.
	ULONG_PTR m_gdiplusToken;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

	sio::client myClient;
	// set infinite reconnect attempts
	myClient.set_reconnect_attempts(999999999999);

	//argv[1] = "myController";
	myClient.connect("https://104.248.124.24:80/socket.io");

	myClient.socket()->emit("join", std::string(argv[1]));

	// setup vigem:
	ControllerManager CM;
	CM.addController();


	// on controller state:
	myClient.socket()->on("controllerState", sio::socket::event_listener_aux([&](std::string const& name, sio::message::ptr const& data, bool isAck, sio::message::list &ack_resp) {

		std::string input = data->get_map()["state"]->get_string();
		std::vector<std::string> inputs;

		std::istringstream iss(input);
		for (std::string input; iss >> input;) {
			inputs.push_back(input);
		}

		std::string btns = inputs[0];
		int LX = std::stoi(inputs[1]);
		int LY = std::stoi(inputs[2]);
		int RX = std::stoi(inputs[3]);
		int RY = std::stoi(inputs[4]);


		printf("%s %d %d %d %d\n", btns.c_str(), LX, LY, RX, RY);

		int dpad = btns[0] - '0';


		long buttons = 0;
		int LT = 0;
		int RT = 0;
		// 0-65536 (-32768)
		double scale = 65536 / 255;
		LX = (int)((LX * scale) - 32768);
		LY = (int)((LY * scale) - 32768);
		RX = (int)((RX * scale) - 32768);
		RY = (int)((RY * scale) - 32768);

		if (dpad == 7) {
			buttons |= XUSB_GAMEPAD_DPAD_UP;
			buttons |= XUSB_GAMEPAD_DPAD_LEFT;
		} else if (dpad == 1) {
			buttons |= XUSB_GAMEPAD_DPAD_UP;
			buttons |= XUSB_GAMEPAD_DPAD_RIGHT;
		} else if (dpad == 5) {
			buttons |= XUSB_GAMEPAD_DPAD_DOWN;
			buttons |= XUSB_GAMEPAD_DPAD_LEFT;
		} else if (dpad == 3) {
			buttons |= XUSB_GAMEPAD_DPAD_DOWN;
			buttons |= XUSB_GAMEPAD_DPAD_RIGHT;
		} else if (dpad == 0) {
			buttons |= XUSB_GAMEPAD_DPAD_UP;
		} else if (dpad == 4) {
			buttons |= XUSB_GAMEPAD_DPAD_DOWN;
		} else if (dpad == 6) {
			buttons |= XUSB_GAMEPAD_DPAD_LEFT;
		} else if (dpad == 2) {
			buttons |= XUSB_GAMEPAD_DPAD_RIGHT;
		} else if (dpad == 8) {
		}

		if (btns[6] == '1') {
			buttons |= XUSB_GAMEPAD_B;
		}
		if (btns[7] == '1') {
			buttons |= XUSB_GAMEPAD_A;
		}
		if (btns[8] == '1') {
			buttons |= XUSB_GAMEPAD_X;
		}
		if (btns[9] == '1') {
			buttons |= XUSB_GAMEPAD_Y;
		}

		if (btns[1] == '1') {
			buttons |= XUSB_GAMEPAD_LEFT_THUMB;
		}
		if (btns[10] == '1') {
			buttons |= XUSB_GAMEPAD_RIGHT_THUMB;
		}

		if (btns[2] == '1') {
			buttons |= XUSB_GAMEPAD_LEFT_SHOULDER;
		}
		if (btns[3] == '1') {
			LT = 255;
		}
		if (btns[11] == '1') {
			buttons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
		}
		if (btns[12] == '1') {
			RT = 255;
		}

		if (btns[4] == '1') {
			buttons |= XUSB_GAMEPAD_BACK;
		}
		if (btns[13] == '1') {
			buttons |= XUSB_GAMEPAD_START;
		}
		if (btns[14] == '1') {
			buttons |= XUSB_GAMEPAD_GUIDE;
		}

		CM.controllers[0].update(buttons, LX, LY, RX, RY, LT, RT);

		//_lock.unlock();

	}));


	// screenshot with new new parameters:
	myClient.socket()->on("screenshot", sio::socket::event_listener_aux([&](std::string const& name, sio::message::ptr const& data, bool isAck, sio::message::list &ack_resp) {

		//printf("recieved screenshot command.\n");

		RECT      rc;
		GetClientRect(GetDesktopWindow(), &rc);

		int x1 = data->get_map()["x1"]->get_int();
		int y1 = data->get_map()["y1"]->get_int();
		int x2 = data->get_map()["x2"]->get_int();
		int y2 = data->get_map()["y2"]->get_int();
		int q = data->get_map()["q"]->get_int();
		int s = data->get_map()["s"]->get_int();

		long width = rc.right;
		long height = rc.bottom;

		POINT a;
		POINT b;

		// in case I want to set the compression level but don't know the width and height:
		if (x1 == -1) {
			width = rc.right;
			height = rc.bottom;

			// for high DPI displays:
			if (width == 1500) {
				width *= 2;
				height *= 2;
			}

			a = { 0, 0 };
			b = { width, height };
		} else {
			a = { x1, y1 };
			b = { x2, y2 };
		}

		//std::string encoded_string = screenshotToBase64Resize(a, b, q, s);
		std::string encoded_string = screenshotToBase64Resize2(a, b, q, s);
		myClient.socket()->emit("screenshot", encoded_string);
	}));


	myClient.socket()->on("quit", sio::socket::event_listener_aux([&](std::string const& name, sio::message::ptr const& data, bool isAck, sio::message::list &ack_resp) {
		exit(0);
	}));


	// Shut Down GDI+
	//Gdiplus::GdiplusShutdown(m_gdiplusToken);


	using namespace std::chrono;
	steady_clock::time_point clock_begin1 = steady_clock::now();
	steady_clock::time_point clock_begin2 = steady_clock::now();

	while (true) {
		Sleep(5000);
		steady_clock::time_point clock_end = steady_clock::now();
		steady_clock::duration time_span;
		double nseconds;

		time_span = clock_end - clock_begin2;
		nseconds = double(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
		//std::cout << nseconds << " seconds." << std::endl;
		if (nseconds > 10) {
			// reset the timer:
			clock_begin2 = steady_clock::now();

			myClient.socket()->emit("join", std::string(argv[1]));

		}
		
		time_span = clock_end - clock_begin1;
		nseconds = double(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
		//std::cout << nseconds << " seconds." << std::endl;
		if (nseconds > 300) {
			//exit(0);
		}
	}

	// why?
	int x;
	std::cin >> x;

	return 0;
}