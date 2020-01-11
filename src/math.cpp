// NOTE(Kay Verbruggen): Uitleg inverse wortels.
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

// NOTE(Kay Verbruggen): Uitleg wortels.
// Als je de inverse wortel keer het originele getal doet, kom je uit bij de normale wortel.
// x * x^(-1/2) = x^(1/2)
// https://en.wikipedia.org/wiki/Fast_inverse_square_root
inline f32 sqrtf(const f32 number) { return number * inv_sqrtf(number); }


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
    
    Vector2f operator*(const f32 &other) { return Vector2f(x * other, y * other); }
    Vector2f operator/(const f32 &other) { return Vector2f(x / other, y / other); }
    
    Vector2f operator+(const Vector2f &other) { return Vector2f(x + other.x, y + other.y); }
    Vector2f operator-(const Vector2f &other) { return Vector2f(x - other.x, y - other.y); }
    Vector2f operator*(const Vector2f &other) { return Vector2f(x * other.x, y * other.y); }
    Vector2f operator/(const Vector2f &other) { return Vector2f(x / other.x, y / other.y); }
    
    bool operator==(const Vector2f &other) { 
        if ((other.x == x) && (other.y == y)) {
            return true;
        }
        return false;
    }
    
    bool operator!=(const Vector2f &other) { 
        if ((other.x == x) && (other.y == y)) {
            return false;
        }
        return true;
    }
    
    f32 Vector2f::length() { return sqrtf(x * x + y * y); }
    Vector2f Vector2f::normalize() { return *this * inv_sqrtf(x * x + y * y); }
};

struct Vector2i {
    Vector2i(i32 ix, i32 iy) {
        x = ix;
        y = iy;
    };
    
    Vector2i(Vector2f v) {
        x = i32(v.x);
        y = i32(v.y);
    }
    
    Vector2i() {
        x = 0;
        y = 0;
    }
    
    i32 x, y;
    
    Vector2i operator*(const i32 &other) { return Vector2i(x * other, y * other); }
    Vector2i operator/(const i32 &other) { return Vector2i(x / other, y / other); }
    
    Vector2i operator+(const Vector2i &other) { return Vector2i(x + other.x, y + other.y); }
    Vector2i operator-(const Vector2i &other) { return Vector2i(x - other.x, y - other.y); }
    Vector2i operator*(const Vector2i &other) { return Vector2i(x * other.x, y * other.y); }
    Vector2i operator/(const Vector2i &other) { return Vector2i(x / other.x, y / other.y); }
    
    bool operator==(const Vector2i &other) { 
        if ((other.x == x) && (other.y == y)) {
            return true;
        }
        return false;
    }
    
    bool operator!=(const Vector2i &other) { 
        if ((other.x == x) && (other.y == y)) {
            return false;
        }
        return true;
    }
    
    
    f32 length() { return sqrtf((f32)(x * x + y * y)); }
    Vector2i normalize() { return *this * (i32)inv_sqrtf((f32)(x * x + y * y)); }
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
    
    Vector3f operator*(const f32 &other) { return Vector3f(x * other, y * other, z * other); }
    Vector3f operator/(const f32 &other) { return Vector3f(x / other, y / other, z / other); }
    
    Vector3f operator+(const Vector3f &other) {
        return Vector3f(x + other.x, y + other.y, z + other.z);
    }
    Vector3f operator-(const Vector3f &other) {
        return Vector3f(x - other.x, y - other.y, z - other.z);
    }
    Vector3f operator*(const Vector3f &other) {
        return Vector3f(x * other.x, y * other.y, z * other.z);
    }
    Vector3f operator/(const Vector3f &other) {
        return Vector3f(x / other.x, y / other.y, z / other.z);
    }
    
    bool operator==(const Vector3f &other) { 
        if ((other.x == x) && (other.y == y) && (other.z == z)) {
            return true;
        }
        return false;
    }
    
    bool operator!=(const Vector3f &other) { 
        if ((other.x == x) && (other.y == y) && (other.z == z)) {
            return false;
        }
        return true;
    }
    
    
    f32 length() { return sqrtf(x * x + y * y + z * z); }
    Vector3f normalize() { return *this * inv_sqrtf(x * x + y * y + z * z); }
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
    
    Vector3i operator*(const i32 &other) { return Vector3i(x * other, y * other, z * other); }
    Vector3i operator/(const i32 &other) { return Vector3i(x / other, y / other, z / other); }
    
    Vector3i operator+(const Vector3i &other) {
        return Vector3i(x + other.x, y + other.y, z + other.z);
    }
    Vector3i operator-(const Vector3i &other) {
        return Vector3i(x - other.x, y - other.y, z - other.z);
    }
    Vector3i operator*(const Vector3i &other) {
        return Vector3i(x * other.x, y * other.y, z * other.z);
    }
    Vector3i operator/(const Vector3i &other) {
        return Vector3i(x / other.x, y / other.y, z / other.z);
    }
    
    
    bool operator==(const Vector3i &other) { 
        if ((other.x == x) && (other.y == y) && (other.z == z)) {
            return true;
        }
        return false;
    }
    
    bool operator!=(const Vector3i &other) { 
        if ((other.x == x) && (other.y == y) && (other.z == z)) {
            return false;
        }
        return true;
    }
    
    
    f32 length() { return sqrtf((f32)(x * x + y * y + z * z)); }
    Vector3i normalize() { return *this * (i32)inv_sqrtf((f32)(x * x + y * y + z * z)); }
};

f32 dot(const Vector2f &u, const Vector2f &v) { return u.x * v.x + u.y * v.y; }
f32 dot(const Vector3f &u, const Vector3f &v) { return u.x * v.x + u.y * v.y + u.z * v.z; }

i32 dot(const Vector2i &u, const Vector2i &v) { return u.x * v.x + u.y * v.y; }
i32 dot(const Vector3i &u, const Vector3i &v) { return u.x * v.x + u.y * v.y + u.z * v.z; }