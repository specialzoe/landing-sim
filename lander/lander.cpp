/*
* Ziel: Steuerung eines Raumfahrzeuges aus einem Orbit um eine Kugel. "Landesimulator".
* 
* Winkel sind stets in rad.
* 
* Gemachte Vereinfacherungen: ###
* 
* Kompilieren am besten über WSL oder in Linux direkt.
*/

#define SCREENSIZE_ROWS 64
#define SCREENSIZE_COLS 32
#define FOV 2.2 // 2.2 rad entsprechen ca. 130°.

#include <iostream>
#include <vector>
#include <stdexcept>
#include <format>

using namespace std;

typedef unsigned char uint8_t;

struct Vector3 {
	double x;
	double y;
	double z;
	Vector3(double x, double y, double z) : x(x), y(y), z(z) {};
	Vector3 operator+(Vector3& v) {
		return Vector3(x+v.x, y+v.y, z+v.z);
	}
	Vector3 operator-(Vector3& v) {
		return Vector3(x - v.x, y - v.y, z - v.z);
	}
	Vector3 operator*(double s) {
		return Vector3(x*s, y*s, z*s);
	}
	Vector3 operator/(double s) {
		return Vector3(x / s, y / s, z / s);
	}
	double dot(Vector3& v) {
		return x * v.x + y * v.y + z * v.z;
	}
	explicit operator string() {
		return format("Vector3[{}, {}, {}]", x, y, z);
	};
};


const uint8_t bayer8[8][8] = { // Die Bayer-Matrix wird für das "ordered-dithering" verwendet.
	{   0, 192,  48, 240,   6, 198,  54, 246 },
	{ 128,  64, 176, 112, 134,  70, 182, 118 },
	{  32, 224,  16, 208,  38, 230,  22, 214 },
	{ 160,  96, 144,  80, 166, 102, 150,  86 },
	{   8, 200,  56, 248,   2, 194,  50, 238 },
	{ 136,  72, 184, 120, 130,  66, 178, 114 },
	{  40, 232,  24, 216,  34, 226,  18, 210 },
	{ 168, 104, 152,  88, 162,  98, 146,  82 }
};


class Screen {
	const size_t  rows;
	const size_t  cols;
	vector<vector<uint8_t>> buffer;
public:
	void set_test_pattern() {
		for (size_t i = 0; i < rows; i++) {
			for (size_t j = 0; j < cols; j++) {
				buffer[i][j] = int(((i * j) / 4) % 256);
			}
		}
	}
	void set_buffer(vector<vector<uint8_t>> input_buffer) {
		if (!input_buffer.empty() &&
			input_buffer.size() == rows &&
			input_buffer[0].size() == cols)
		{
			buffer = input_buffer;

		}
		else throw invalid_argument("buffers have different dimensions.");
	}
	void dither() {
		for (size_t i = 0; i < rows; i++) {
			for (size_t j = 0; j < cols; j++) {
				buffer[i][j] = bayer8[i % 8][j % 8] < buffer[i][j];
			}
		}
	}
	void print_pixels() {
		cout << "-----begin half-block print-----" << endl;
		for (size_t i = 0; i < (rows / 2); i++) {
			for (size_t j = 0; j < (cols); j++) {
				if (buffer[i * 2][j] && buffer[(i * 2) + 1][j]) printf("█");
				else if (buffer[i * 2][j]) printf("▀");
				else if (buffer[(i * 2) + 1][j]) printf("▄");
				else printf(" ");
			} cout << endl;
		}
		if (rows % 2) { // ggf. ungerade letzte Zeile
			for (size_t j = 0; j < (cols); j++) {
				if (buffer[rows - 1][j]) printf("▀");
				else printf(" ");
			} cout << endl;
		}
		cout << "-----end half-block print-----" << endl;
	}
	Screen(size_t rows, size_t cols) : rows(rows), cols(cols), buffer(rows, vector<uint8_t>(cols)) {};
};

class Camera {
	Vector3 position;
	Vector3 orientation;
	double fov;
	double screen_distance;
public:
	Camera(Vector3 position, Vector3 orientation, double fov, double screen_distance)
		:position(position), orientation(orientation), fov(fov), screen_distance(screen_distance) {};
};


int main(void) {
	Screen screen(SCREENSIZE_ROWS, SCREENSIZE_COLS);
	Camera camera(Vector3(20.0, 20.0, 0.0), Vector3(0.0, 20.0, 0.0), FOV, 10.0);
	

	return EXIT_SUCCESS;
}