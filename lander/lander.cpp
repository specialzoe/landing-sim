/*
* Ziel: Steuerung eines Raumfahrzeuges aus einem Orbit um eine Kugel. "Landesimulator".
* Kompilieren am besten mit g++ mit -std=c++20 flag.
*/

#define SCREENSIZE_ROWS 200
#define SCREENSIZE_COLS 256

#include <iostream>
#include <vector>
#include <stdexcept>
#include <format>
#include <math.h>

using namespace std;

typedef unsigned char uint8_t;
typedef vector<vector<uint8_t>> pixel_buffer;
struct Vector3 {
	double x;
	double y;
	double z;
	Vector3(double x, double y, double z) : x(x), y(y), z(z) {};
	Vector3(void) : x(0.0), y(0.0), z(0.0) {};
	Vector3 operator+(Vector3 v) {
		return Vector3(x+v.x, y+v.y, z+v.z);
	}
	Vector3 operator-(Vector3 v) {
		return Vector3(x - v.x, y - v.y, z - v.z);
	}
	Vector3 operator*(double s) {
		return Vector3(x*s, y*s, z*s);
	}
	Vector3 operator/(double s) {
		return Vector3(x / s, y / s, z / s);
	}
	double dot(Vector3 v) {
		return x * v.x + y * v.y + z * v.z;
	}
	double len() {
		return sqrt(x * x + y * y + z * z);
	}
	Vector3 norm() {
		double l = len();
		return Vector3(x / l, y / l, z / l);
	}
	explicit operator string() {
		return format("Vector3[{}, {}, {}]", x, y, z);
	};
};
struct Basis3 {
	Vector3 e_x;
	Vector3 e_y;
	Vector3 e_z;
	Basis3(Vector3 e_x, Vector3 e_y, Vector3 e_z) : e_x(e_x), e_y(e_y), e_z(e_z) {};
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
	pixel_buffer buffer;
public:
	size_t get_rows() { return rows; }
	size_t get_cols() { return cols; }
	void set_test_pattern() {
		for (size_t i = 0; i < rows; i++) {
			for (size_t j = 0; j < cols; j++) {
				buffer[i][j] = int(((i * j) / 4) % 256);
			}
		}
	}
	void set_buffer(pixel_buffer input_buffer) {
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
	void print_values() {
		cout << "-----begin value print-----" << endl;
		for (size_t i = 0; i < rows; i++) {
			for (size_t j = 0; j < cols; j++) {
				printf("%3d ", buffer[i][j]);
			} cout << endl;
		}
		cout << "-----end value print-----" << endl;
	}
	Screen(size_t rows, size_t cols) : rows(rows), cols(cols), buffer(rows, vector<uint8_t>(cols)) {};
};

class Sphere {
	Vector3 position;
	double radius;
public:
	Vector3 get_position() { return position; }
	double get_radius() { return radius; }
	Sphere(Vector3 position, double radius) : position(position), radius(radius) {};
};

class Camera {
	Vector3 position;
	Basis3 basis; // Normiert
	size_t buffer_rows;
	size_t buffer_cols;
	double screen_distance;
public:
	pixel_buffer render_sphere(Sphere sphere) {
		Vector3 d; // Strahlrichtung
		Vector3 v = position - sphere.get_position(); // Mittelpunkt Sphäre -> Kamera
		double r = sphere.get_radius(); // Radius
		double y_offset = double(buffer_cols) / 2.0;
		double z_offset = double(buffer_rows) / 2.0;

		pixel_buffer buffer(buffer_rows, vector<uint8_t>(buffer_cols));

		for (size_t i = 0; i < buffer_rows; i++) {
			for (size_t j = 0; j < buffer_cols; j++) {
				// Basiswechsel der Bildpunkte in globale Koordinaten
				d = basis.e_x * screen_distance +
					basis.e_y * (j + 0.5 - y_offset) +
					basis.e_z * (i + 0.5 - z_offset);
					double diskriminante = (4 * v.dot(d) * v.dot(d)) - (4 * d.dot(d) * ((v.dot(v)) - (r * r)));
				
				if (diskriminante < 0.0) buffer[i][j] = 0;
				else {
					double t; // Parameter der Geradengleichung
					if (diskriminante == 0.0) { // Eine Lösung für t
						t = (-1.0 * v.dot(d)) / (d.dot(d));
					}
					else { // 2 Lösungen (kürzerer Abstand gesucht)
						double t1 = ((-2.0 * v.dot(d)) + sqrt(diskriminante)) / (2.0 * d.dot(d));
						double t2 = ((-2.0 * v.dot(d)) - sqrt(diskriminante)) / (2.0 * d.dot(d));
						if (t1 > 0 && t2 > 0) t = min(t1, t2);
						else if (t1 > 0) t = t1;
						else if (t2 > 0) t = t2;
						else { // beide hinter der Kamera
							buffer[i][j] = 0;
							continue;
						}
					}
					Vector3 n = (d * t + v).norm();
					double brightness = max(Vector3(0, 255, 0).dot(n), 0.0);
					buffer[i][j] = brightness;
				}
			}
		}
		return buffer;
	}
	Vector3 get_position() { return position; }
	Camera(Vector3 position, Basis3 basis, size_t buffer_rows, size_t buffer_cols, double screen_distance)
		:position(position), basis(basis), buffer_rows(buffer_rows), buffer_cols(buffer_cols), screen_distance(screen_distance) {};
};


int main(void) {
	Screen screen(SCREENSIZE_ROWS, SCREENSIZE_COLS);
	Camera camera(Vector3(100.0, 0.0, 0.0), Basis3(Vector3(-1,-1,0).norm(), Vector3(-1, 1, 0).norm(), Vector3(0, 0, 1).norm()), screen.get_rows(), screen.get_cols(), 100.0);
	Sphere sphere(Vector3(0,-70,-20), 75.0);

	screen.set_buffer(camera.render_sphere(sphere));
	screen.dither();
	screen.print_pixels();

	return EXIT_SUCCESS;
}