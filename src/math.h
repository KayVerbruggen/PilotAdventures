#pragma once

// NOTE: Uitleg Vector2f.
//
struct Vector2f {
    Vector2f(f32 ix, f32 iy) {
        x = ix;
        y = iy;
    };
    Vector2f() {
        x = 0.0f;
        y = 0.0f;
    }
    f32 x, y;

    // Vector2f en f32.
    Vector2f operator*(const f32 &other);
    Vector2f operator/(const f32 &other);

    // Vector2f met zichzelf.
    Vector2f operator-(const Vector2f &other);
    Vector2f operator+(const Vector2f &other);
    Vector2f operator*(const Vector2f &other);
    Vector2f operator/(const Vector2f &other);

    Vector2f normalize();
    f32 length();
};

struct Vector2i {
    Vector2i(i32 ix, i32 iy) {
        x = ix;
        y = iy;
    };
    Vector2i() {
        x = 0;
        y = 0;
    }
    i32 x, y;

    // Vector2i en f32.
    Vector2i operator*(const i32 &other);
    Vector2i operator/(const i32 &other);

    // Vector2i met zichzelf.
    Vector2i operator-(const Vector2i &other);
    Vector2i operator+(const Vector2i &other);
    Vector2i operator*(const Vector2i &other);
    Vector2i operator/(const Vector2i &other);

    Vector2i normalize();
    f32 length();
};

struct Vector3f {
    Vector3f(f32 ix, f32 iy, f32 iz) {
        x = ix;
        y = iy;
        z = iz;
    }
    Vector3f() {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    }
    f32 x, y, z;

    // Vector3f en f32.
    Vector3f operator*(const f32 &other);
    Vector3f operator/(const f32 &other);

    // Vector3f met zichzelf.
    Vector3f operator-(const Vector3f &other);
    Vector3f operator+(const Vector3f &other);
    Vector3f operator*(const Vector3f &other);
    Vector3f operator/(const Vector3f &other);

    Vector3f normalize();
    f32 length();
};

struct Vector3i {
    Vector3i(i32 ix, i32 iy, i32 iz) {
        x = ix;
        y = iy;
        z = iz;
    }
    Vector3i() {
        x = 0;
        y = 0;
        z = 0;
    }
    i32 x, y, z;

    // Vector3i en i32.
    Vector3i operator*(const i32 &other);
    Vector3i operator/(const i32 &other);

    // Vector3i met zichzelf.
    Vector3i operator-(const Vector3i &other);
    Vector3i operator+(const Vector3i &other);
    Vector3i operator*(const Vector3i &other);
    Vector3i operator/(const Vector3i &other);

    Vector3i normalize();
    f32 length();
};

f32 sqrtf(const f32 number);
f32 inv_sqrtf(const f32 number);

f32 dot(const Vector2f &u, const Vector2f &v);
i32 dot(const Vector2i &u, const Vector2i &v);
f32 dot(const Vector3f &u, const Vector3f &v);
i32 dot(const Vector3i &u, const Vector3i &v);
