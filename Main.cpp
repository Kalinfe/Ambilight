#pragma warning(disable : 4996)

#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <CUESDK.h>
#include <Windows.h>
#include "linmath.h"

const char* toString(CorsairError error)
{
	switch (error) {
	case CE_Success:
		return "CE_Success";
	case CE_ServerNotFound:
		return "CE_ServerNotFound";
	case CE_NoControl:
		return "CE_NoControl";
	case CE_ProtocolHandshakeMissing:
		return "CE_ProtocolHandshakeMissing";
	case CE_IncompatibleProtocol:
		return "CE_IncompatibleProtocol";
	case CE_InvalidArguments:
		return "CE_InvalidArguments";
	default:
		return "unknown error";
	}
}

double getKeyboardWidth(CorsairLedPositions* ledPositions)
{
	const auto minmaxLeds = std::minmax_element(ledPositions->pLedPosition, ledPositions->pLedPosition + ledPositions->numberOfLed,
		[](const CorsairLedPosition& clp1, const CorsairLedPosition& clp2) {
			return clp1.left < clp2.left;
		});
	return minmaxLeds.second->left + minmaxLeds.second->width - minmaxLeds.first->left;
}

double getKeyboardHeight(CorsairLedPositions* ledPositions)
{
	const auto minmaxLeds = std::minmax_element(ledPositions->pLedPosition, ledPositions->pLedPosition + ledPositions->numberOfLed,
		[](const CorsairLedPosition& clp1, const CorsairLedPosition& clp2) {
			return clp1.top < clp2.top;
		});
	return minmaxLeds.second->top + minmaxLeds.second->height - minmaxLeds.first->top;
}

struct screenLed {
	CorsairLedId ledId;
	int screenX;
	int screenY;
};

void getKeyboardLedInfo(int sWidth, int sHeight, struct screenLed* l, CorsairLedPositions* ledPos) {
	std::cout << sWidth << " " << sHeight << std::endl;
	double ledWidth = getKeyboardWidth(ledPos);
	double ledHeight = getKeyboardHeight(ledPos);
	for (auto i = 0; i < ledPos->numberOfLed; i++) {
		const auto led = ledPos->pLedPosition[i];
		double relPosX = (led.left + led.width / 2) / ledWidth;
		double relPosY = (led.top + led.height / 2) / ledHeight;
		int nearX = (int)(relPosX * sWidth);
		int nearY = (int)(relPosY * sHeight);
		l[i].ledId = led.ledId;
		l[i].screenX = nearX;
		l[i].screenY = nearY;
	}
}

void getSmoothYPixel(int pX, int pY, RGBQUAD pPixels[], int scrnWidth, int scrnHeight, int smoothing, float* red, float* green, float* blue) {
	long r = 0, g = 0, b = 0;
	int p = (scrnHeight - pY - 1) * scrnWidth + pX;
	p -= smoothing * scrnWidth;
	int size = scrnWidth * scrnHeight;
	int counts = smoothing * 2 + 1;
	for (int i = 0; i <= smoothing*2; i++) {
		int pP = p + i * scrnWidth;
		if (pP >= size || pP < 0) {
			counts--;
		}
		else {
			r += pPixels[pP].rgbRed;
			g += pPixels[pP].rgbGreen;
			b += pPixels[pP].rgbBlue;
		}
	}
	*red = (r / (255.0 * counts));
	*green = (g / (255.0 * counts));
	*blue = (b / (255.0 * counts));
}

void writeSideGlare(RGBQUAD* pPixels, GLFWwindow* win, int scrnHeight, int scrnWidth) {
	int width, height;
	glfwMakeContextCurrent(win);
	glfwGetWindowSize(win, &width, &height);
	for (int y = 0; y < height; y ++) {
		float placeY = (y * 2.0) / height - 1;
		float r, g, b;
		getSmoothYPixel(scrnWidth-1, y, pPixels, scrnWidth, scrnHeight, 20, &r, &g, &b);
		glBegin(GL_LINES);
		glColor3f(r, g, b);
		glVertex2f(-1, -placeY);
		glColor3f(0, 0, 0);
		glVertex2f(0, -placeY);
		glEnd();
	}
	glfwSwapBuffers(win);
	glfwPollEvents();
}

void writeSideGlare2(RGBQUAD* pPixels, GLFWwindow* win, int scrnHeight, int scrnWidth) {
	int width, height;
	glfwMakeContextCurrent(win);
	glfwGetWindowSize(win, &width, &height);
	for (int y = 0; y < height; y++) {
		int levelY = y * (scrnHeight / (float)height);
		float placeY = (y * 2.0) / height - 1;
		float r, g, b;
		getSmoothYPixel(0, levelY, pPixels, scrnWidth, scrnHeight, 20, &r, &g, &b);
		glBegin(GL_LINES);
		glColor3f(r, g, b);
		glVertex2f(1, -placeY);
		glColor3f(0, 0, 0);
		glVertex2f(0, -placeY);
		glEnd();
	}
	glfwSwapBuffers(win);
	glfwPollEvents();
}

void writeMonitorToKeyboard(struct screenLed* l, int ledCount, GLFWwindow* win, GLFWwindow* win2, HDC desktopDC, HDC captureDC, HBITMAP captureMap, BITMAPINFO bmi, RGBQUAD* pPixels, int scrnWidth, int scrnHeight) {
	SelectObject(captureDC, captureMap);

	BitBlt(captureDC, 0, 0, scrnWidth, scrnHeight, desktopDC, 0, 0, SRCCOPY | CAPTUREBLT);

	GetDIBits(captureDC, captureMap, 0, scrnHeight, pPixels, &bmi, DIB_RGB_COLORS);

	std::vector<CorsairLedColor> vec;

	long aR = 0, aG = 0, aB = 0;

	for (auto i = 0; i < ledCount; i++) {
		const auto led = l[i];
		CorsairLedColor color = CorsairLedColor();
		color.ledId = led.ledId;
		int p = (scrnHeight - led.screenY - 1) * scrnWidth + led.screenX;
		int r = pPixels[p].rgbRed;
		int g = pPixels[p].rgbGreen;
		int b = pPixels[p].rgbBlue;
		color.r = r;
		color.g = g;
		color.b = b;
		aR += r;
		aG += g;
		aB += b;
		vec.push_back(color);
	}

	aR /= ledCount;
	aG /= ledCount;
	aB /= ledCount;

	CorsairLedColor c = CorsairLedColor();
	c.ledId = CLM_1;
	c.r = aR;
	c.g = aG;
	c.b = aB;
	vec.push_back(c);
	CorsairLedColor c3 = CorsairLedColor();
	c3.ledId = CLM_3;
	c3.r = aR;
	c3.g = aG;
	c3.b = aB;
	vec.push_back(c3);

	CorsairSetLedsColors(static_cast<int>(vec.size()), vec.data());

	if(win != NULL)
		writeSideGlare(pPixels, win, scrnHeight, scrnWidth);
	if(win2 != NULL)
		writeSideGlare2(pPixels, win2, scrnHeight, scrnWidth);
}

int main() {
	CorsairPerformProtocolHandshake();
	if (const auto error = CorsairGetLastError()) {
		std::cout << "Handshake failed: " << toString(error) << std::endl;
		return -1;
	}

	const auto ledPositions = CorsairGetLedPositions();
	if (!ledPositions || ledPositions->numberOfLed < 0) {
		return 1;
	}

	struct screenLed* l = (screenLed*)malloc(ledPositions->numberOfLed * sizeof(struct screenLed));

	::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

	int scrnWidth = GetSystemMetrics(SM_CXSCREEN);
	int scrnHeight = GetSystemMetrics(SM_CYSCREEN);

	getKeyboardLedInfo(scrnWidth, scrnHeight, l, ledPositions);

	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	int count;
	GLFWmonitor** monitors = glfwGetMonitors(&count);
	std::cout << count << std::endl;
	const GLFWvidmode* mode = glfwGetVideoMode(monitors[1]);
	int xpos, ypos;
	glfwGetMonitorPos(monitors[1], &xpos, &ypos);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "MonitorSideEffect", nullptr, NULL);
	glfwSetWindowMonitor(window, NULL, xpos, ypos, mode->width - 1, mode->height, mode->refreshRate);
	GLFWwindow* win2 = NULL;
	if (count > 2) {
		const GLFWvidmode* mode2 = glfwGetVideoMode(monitors[2]);
		glfwGetMonitorPos(monitors[2], &xpos, &ypos);
		win2 = glfwCreateWindow(mode2->width, mode2->height, "MonitorSideEffect2", monitors[2], NULL);
		glfwSetWindowMonitor(win2, NULL, xpos + 1, 0, mode2->width - 1, mode2->height, mode->refreshRate);
	}
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	HWND desktop = GetDesktopWindow();
	HDC desktopDC = GetDC(desktop);
	HDC captureDC = CreateCompatibleDC(desktopDC);
	HBITMAP captureMap = CreateCompatibleBitmap(desktopDC, scrnWidth, scrnHeight);
	SelectObject(captureDC, captureMap);

	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = scrnWidth;
	bmi.bmiHeader.biHeight = scrnHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	RGBQUAD* pPixels = new RGBQUAD[scrnWidth * scrnHeight];

	while (true) {
		glClear(GL_COLOR_BUFFER_BIT);
		writeMonitorToKeyboard(l, ledPositions->numberOfLed, window, win2, desktopDC, captureDC, captureMap, bmi, pPixels, scrnWidth, scrnHeight);
		//std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	delete[] pPixels;

	ReleaseDC(desktop, desktopDC);
	DeleteDC(captureDC);
	DeleteObject(captureMap);

	glfwDestroyWindow(window);
	glfwTerminate;
	exit(EXIT_SUCCESS);
}