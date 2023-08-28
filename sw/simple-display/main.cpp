#include <math.h>

#include <climits>
#include <cstring>
#include <iostream>

#include "libDisplay.hpp"

Framebuffer* volatile const framebuffer = (Framebuffer* volatile const)(0x72000000);

typedef Framebuffer::Point Point;
typedef Framebuffer::PointF PointF;
typedef Framebuffer::Color Color;
static constexpr auto screenWidth = Framebuffer::screenWidth;
static constexpr auto screenHeight = Framebuffer::screenHeight;

void activeWait(float factor = 0.5) {
	for (uint16_t i = 0; i < UINT16_MAX * factor; i++) {
	};
}

Point getRandomPoint() {
	return Point(rand() % screenWidth, rand() % screenHeight);
}

Color getRandomColor() {
	return rand() % SHRT_MAX;
}

void drawBackground() {
	for (uint32_t i = 0; i < screenHeight - 1; i++) {
		display::drawLine(Framebuffer::Type::background, Point(0, i), Point(screenWidth, i),
		                  fromRGB(i & 0x7, 0, i >> 3));
		if (i % 5 == 0)
			display::applyFrame();
	}
}

void drawFunnyRects() {
	static uint32_t m = 0;
	for (uint16_t i = 0; i < 400; i++) {
		// drawLine(framebuffer->getInactiveFrame(), getRandomPoint(),
		// getRandomPoint(), getRandomColor());
		display::drawRect(Framebuffer::Type::foreground, Point(m % screenWidth, m % screenHeight),
		                  Point((screenWidth - (m + 1) % screenWidth), (screenHeight - (m + 1) % screenHeight)),
		                  fromRGB(255 - (m % 255), (m % 255), i % 255));
		display::applyFrame();
		activeWait(.005);
		m += 17;
	}
}

/**
 * @param completion Values between 0..1
 */
void progressBar(bool horizontal, Framebuffer::PointF base, Framebuffer::PointF extent, Color fg, Color bg,
                 float completion) {
	PointF progress = horizontal ? PointF(extent.x * completion, extent.y) : PointF(extent.x, extent.y * completion);

	display::fillRect(Framebuffer::Type::foreground, base, base + progress, fg);
	display::fillRect(Framebuffer::Type::foreground, base + progress, base + extent, bg);
}

void drawFunnyBar(bool horizontal = false) {
	Color bgColor = fromRGB(0, 0, 1);
	Color progressColor = fromRGB(126, 255, 10);
	Point baseOL;
	Point baseUR;
	uint16_t stepsize = 1;
	if (horizontal) {
		baseOL = Point(100, 250);
		baseUR = Point(700, 350);
	} else {
		baseOL = Point(350, 50);
		baseUR = Point(450, 550);
	}

	display::fillRect(Framebuffer::Type::foreground, baseOL, baseUR, bgColor);
	display::applyFrame();
	Point barBase(baseOL.x + 10, baseOL.y + 10);
	Point barExtent((baseUR.x - baseOL.x) - 20, (baseUR.y - baseOL.y) - 20);

	for (float progress = 0; progress <= 1; progress += 0.01) {
		display::fillRect(Framebuffer::Type::foreground, baseOL, baseUR, bgColor);
		progressBar(horizontal, barBase, barExtent, progressColor, bgColor, progress);
		display::applyFrame();
		activeWait(.005);
	}
	for (float progress = 1; progress >= 0; progress -= 0.01) {
		display::fillRect(Framebuffer::Type::foreground, baseOL, baseUR, bgColor);
		progressBar(horizontal, barBase, barExtent, progressColor, bgColor, progress);
		display::applyFrame();
		activeWait(.005);
	}
}

using namespace std;

int main() {
	cout << "draw background" << endl;
	drawBackground();
	while (true) {
		cout << "Draw H-bar" << endl;
		drawFunnyBar(true);
		display::fillFrame(Framebuffer::Type::foreground);
		display::applyFrame();
		cout << "Draw V-bar" << endl;
		drawFunnyBar(false);
		display::fillFrame(Framebuffer::Type::foreground);
		display::applyFrame();
		cout << "Draw Rects" << endl;
		drawFunnyRects();
		display::fillFrame(Framebuffer::Type::foreground);
		display::applyFrame();
	}
	return 0;
}
