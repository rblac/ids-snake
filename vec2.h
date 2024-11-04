struct vec2 {
	int x, y;

	vec2(int x = 0, int y = 0): x(x), y(y) {}

	inline bool isZero() { return x == 0 && y == 0; }
};
inline vec2 operator+(const vec2& lhs, const vec2& rhs) { return vec2(lhs.x+rhs.x, lhs.y+rhs.y); }
inline vec2 operator-(const vec2& lhs, const vec2& rhs) { return vec2(lhs.x-rhs.x, lhs.y-rhs.y); }
inline vec2 operator*(const vec2& lhs, const int& s) { return vec2(lhs.x*s, lhs.y*s); }
inline vec2 operator*(const int& s, const vec2& rhs) { return vec2(rhs.x*s, rhs.y*s); }
inline vec2 operator/(const vec2& lhs, const int& s) { return vec2(lhs.x/s, lhs.y/s); }

inline bool operator==(const vec2& lhs, const vec2& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
inline bool operator!=(const vec2& lhs, const vec2& rhs) { return (lhs == rhs) != true; }

inline void printVec(vec2 v) {
	Serial.print("[ ");
	Serial.print(v.x);
	Serial.print(", ");
	Serial.print(v.y);
	Serial.print(" ]");
}
