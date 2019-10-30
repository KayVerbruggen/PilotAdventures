#include "math.h"

// NOTE: Uitleg inverse wortels.
// Dit is de inverse wortel dat wil zeggen 1 / x^(1/2) oftewel x^(-1/2).
// Dit is de inverse wortelfunctie die werd gebruikt in Quake 3.
// Dit is ongeveer 3x sneller dan de standaard wortelfunctie van C++,
// en nog steeds super nauwkeurig: 16^-0.5 = 0.249999 i.p.v 0.25
// https://en.wikipedia.org/wiki/Fast_inverse_square_root
inline f32 inv_sqrtf(const f32 number) {
    f32 xhalf = number * 0.5f;
    const f32 threehalfs = 1.5f;

    union {
        f32 x;
        long i;
    } u;

    u.x = number;
    u.i = 0x5f3759df - (u.i >> 1);                // what the fuck?
    u.x = u.x * (threehalfs - xhalf * u.x * u.x); // 1st iteration
    u.x = u.x * (threehalfs - xhalf * u.x * u.x); // 2nd iteration

    return u.x;
}

// NOTE: Uitleg wortels.
// Als je de inverse wortel keer het originele getal doet, kom je uit bij de normale wortel.
// x * x^(-1/2) = x^(1/2)
// https://en.wikipedia.org/wiki/Fast_inverse_square_root
inline f32 sqrtf(const f32 number) { return number * inv_sqrtf(number); }

// Vector2f
Vector2f Vector2f::operator*(const f32 &other) { return Vector2f(x * other, y * other); }
Vector2f Vector2f::operator/(const f32 &other) { return Vector2f(x / other, y / other); }

Vector2f Vector2f::operator+(const Vector2f &other) { return Vector2f(x + other.x, y + other.y); }
Vector2f Vector2f::operator-(const Vector2f &other) { return Vector2f(x - other.x, y - other.y); }
Vector2f Vector2f::operator*(const Vector2f &other) { return Vector2f(x * other.x, y * other.y); }
Vector2f Vector2f::operator/(const Vector2f &other) { return Vector2f(x / other.x, y / other.y); }

f32 dot(const Vector2f &u, const Vector2f &v) { return u.x * v.x + u.y * v.y; }
f32 Vector2f::length() { return sqrtf(x * x + y * y); }
Vector2f Vector2f::normalize() { return *this * inv_sqrtf(x * x + y * y); }

// Vector2i
Vector2i Vector2i::operator*(const i32 &other) { return Vector2i(x * other, y * other); }
Vector2i Vector2i::operator/(const i32 &other) { return Vector2i(x / other, y / other); }

Vector2i Vector2i::operator+(const Vector2i &other) { return Vector2i(x + other.x, y + other.y); }
Vector2i Vector2i::operator-(const Vector2i &other) { return Vector2i(x - other.x, y - other.y); }
Vector2i Vector2i::operator*(const Vector2i &other) { return Vector2i(x * other.x, y * other.y); }
Vector2i Vector2i::operator/(const Vector2i &other) { return Vector2i(x / other.x, y / other.y); }

i32 dot(const Vector2i &u, const Vector2i &v) { return u.x * v.x + u.y * v.y; }
f32 Vector2i::length() { return sqrtf((f32)(x * x + y * y)); }
Vector2i Vector2i::normalize() { return *this * (i32)inv_sqrtf((f32)(x * x + y * y)); }

// Vector3f
Vector3f Vector3f::operator*(const f32 &other) { return Vector3f(x * other, y * other, z * other); }
Vector3f Vector3f::operator/(const f32 &other) { return Vector3f(x / other, y / other, z / other); }

Vector3f Vector3f::operator+(const Vector3f &other) {
    return Vector3f(x + other.x, y + other.y, z + other.z);
}
Vector3f Vector3f::operator-(const Vector3f &other) {
    return Vector3f(x - other.x, y - other.y, z - other.z);
}
Vector3f Vector3f::operator*(const Vector3f &other) {
    return Vector3f(x * other.x, y * other.y, z * other.z);
}
Vector3f Vector3f::operator/(const Vector3f &other) {
    return Vector3f(x / other.x, y / other.y, z / other.z);
}

f32 dot(const Vector3f &u, const Vector3f &v) { return u.x * v.x + u.y * v.y + u.z * v.z; }
f32 Vector3f::length() { return sqrtf(x * x + y * y + z * z); }
Vector3f Vector3f::normalize() { return *this * inv_sqrtf(x * x + y * y + z * z); }

// Vector3i
Vector3i Vector3i::operator*(const i32 &other) { return Vector3i(x * other, y * other, z * other); }
Vector3i Vector3i::operator/(const i32 &other) { return Vector3i(x / other, y / other, z / other); }

Vector3i Vector3i::operator+(const Vector3i &other) {
    return Vector3i(x + other.x, y + other.y, z + other.z);
}
Vector3i Vector3i::operator-(const Vector3i &other) {
    return Vector3i(x - other.x, y - other.y, z - other.z);
}
Vector3i Vector3i::operator*(const Vector3i &other) {
    return Vector3i(x * other.x, y * other.y, z * other.z);
}
Vector3i Vector3i::operator/(const Vector3i &other) {
    return Vector3i(x / other.x, y / other.y, z / other.z);
}

i32 dot(const Vector3i &u, const Vector3i &v) { return u.x * v.x + u.y * v.y + u.z * v.z; }
f32 Vector3i::length() { return sqrtf((f32)(x * x + y * y + z * z)); }
Vector3i Vector3i::normalize() { return *this * (i32)inv_sqrtf((f32)(x * x + y * y + z * z)); }