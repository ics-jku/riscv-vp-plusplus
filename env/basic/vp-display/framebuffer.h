#pragma once
#include <inttypes.h>

#include <cassert>
#include <cstring>

#define SHMKEY 1338

struct Framebuffer {
	static constexpr uint16_t screenWidth = 800;
	static constexpr uint16_t screenHeight = 600;

	typedef uint16_t Color;

	struct Point {
		uint32_t x;
		uint32_t y;
		inline Point() : x(0), y(0){};
		inline Point(uint32_t x, uint32_t y) : x(x), y(y){};
	};

	struct PointF {
		float x;
		float y;
		inline PointF() : x(0), y(0){};
		inline PointF(float x, float y) : x(x), y(y){};
		inline PointF(Point p) : x(p.x), y(p.y){};
	};

	struct Frame {
		Color raw[screenHeight][screenWidth];  // Notice: Screen is on side
	};

	enum class Type : uint8_t { foreground, background };
	uint8_t activeFrame;
	enum class Command : uint8_t {
		none = 0,
		clearAll,
		fillFrame,
		applyFrame,
		drawLine,
	} volatile command;
	union Parameter {
		struct {
			// fillframe
			Type frame;
			Color color;
		} fill;
		struct {
			// drawLine
			Type frame;
			PointF from;
			PointF to;
			Color color;
		} line;
		inline Parameter(){};
	} parameter;
	Frame frames[2];
	Frame background;

	Framebuffer() : activeFrame(0), command(Command::none){};

	Frame& getActiveFrame() {
		return frames[activeFrame % 2];
	}
	Frame& getInactiveFrame() {
		return frames[(activeFrame + 1) % 2];
	}
	Frame& getBackground() {
		return background;
	}
	Frame& getFrame(Type type) {
		if (type == Type::foreground)
			return getInactiveFrame();
		else if (type == Type::background)
			return getBackground();

		assert(false && "Get invalid frame type");
		return background;
	}
};

inline Framebuffer::PointF operator+(const Framebuffer::PointF l, Framebuffer::PointF const r) {
	return Framebuffer::PointF(l.x + r.x, l.y + r.y);
}
