#pragma once

#include "tagged.h"

#include <compare>
#include <iostream>
#include <cmath>

namespace geom {

    inline double Round6(double v) {
        return std::round(v * 1'000'000.0) / 1'000'000.0;
    }

    using Dimension = double;
    using Coord = Dimension;

    struct Position {
        Coord x, y;

        Position() = default;
        Position(double nx, double ny)
            : x(nx)
            , y(ny) {
        }

        bool operator==(const Position& other) const noexcept {
            return (x == other.x) && (y == other.y);
        }

        bool operator!=(const Position& other) const noexcept {
            return !(*this == other);
        }

        Position& operator*=(double scale) {
            x *= scale;
            y *= scale;
            return *this;
        }

        Position& operator+=(const Position& rhs) {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        auto operator<=>(const Position&) const = default;
    };

    inline Position operator*(Position lhs, double rhs) {
        return lhs *= rhs;
    }

    inline Position operator*(double lhs, Position rhs) {
        return rhs *= lhs;
    }

    inline Position operator+(Position lhs, const Position& rhs) {
        return lhs += rhs;
    }

    inline Position operator+(const Position& lhs, Position rhs) {
        return rhs += lhs;
    }

    using Point = Position;
    using Point2D = Position;
    using Vect2D = Position;

    struct Size {
        Dimension width, height;
    };

    struct Rectangle {
        Position position;
        Size size;
    };

    struct Offset {
        Dimension dx, dy;
    };

    struct Speed {
        double vx, vy;
    };

    struct MoveResult {
        Position position;
        bool hit_boundary;
    };

    struct Loot {
        using Id = util::Tagged<size_t, Loot>;
        Id id;
        size_t type;
        Position position;
        int value = 0;

        Loot() = default;

        Loot(Id id, size_t type, Position position, int value = 0)
            : id(std::move(id))
            , type(type)
            , position(position)
            , value(value) {
        }
    };

    enum class Direction {
        NORTH,
        SOUTH,
        WEST,
        EAST
    };

    inline double Dot(const Speed& a, const Speed& b) {
        return a.vx * b.vx + a.vy * b.vy;
    }

    inline double SqLength(const Speed& s) {
        return s.vx * s.vx + s.vy * s.vy;
    }

    inline double SqLength(const Position& p) {
        return p.x * p.x + p.y * p.y;
    }

    inline std::ostream& operator<<(std::ostream& os, const Position& pos) {
        os << "(" << pos.x << ", " << pos.y << ")";
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const Speed& speed) {
        os << "(" << speed.vx << ", " << speed.vy << ")";
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const Size& size) {
        os << "(" << size.width << "x" << size.height << ")";
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const Rectangle& rect) {
        os << "geom::Rectangle{position: " << rect.position << ", size: " << rect.size << "}";
        return os;
    }

}  // namespace geom