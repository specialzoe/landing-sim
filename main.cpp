/*
* Ziel: Steuerung eines Raumfahrzeuges aus einem Orbit um eine Kugel. "Landesimulator". (wird wohl etwas verfehlt ^-^ leider wenig Zeit :c)
* Kompilieren am besten mit g++ mit -std=c++20 flag.
* ----- Keymap -----
* HJKLIM -> Translation
* WS -> Nicken / pitch
* QE -> Rollen / roll
* AD -> Gieren / yaw
*/

#define SCREENSIZE_ROWS 80
#define SCREENSIZE_COLS 200
#define PRINT_VALUES 0 //Ausgabetyp
#define PRINT_PIXELS 1 //Ausgabetyp
#define TRANSLATION_UNIT 100
#define ROTATION_UNIT 1

#include <iostream>
#include <vector> // VLA
#include <stdexcept>
#include <format> // Format strings
#include <math.h>
#include <chrono> // deltatime und FPS
#include <thread> // graceful shutdown
#include <csignal> // SIGINT abfangen
#include <atomic>
#include <sys/select.h> // Eingabe non-blocking
#include <unistd.h> // Eingabe non-blocking

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
struct Matrix3 {
	double e[3][3];

	Matrix3() { // Alle Array-Werte auf 0 setzen TODO vereinfachung durch initializer list prüfen
		for (int i = 0; i<3; i++) {
			for (int j = 0; j<3; j++) {
				e[i][j]=0.0;
			}
		}
	}

	Matrix3(initializer_list<double> initial_values) {
		auto a = initial_values.begin(); // Erstes Element der Übergebenen Liste
		for (int i = 0; i<3; i++) {
			for (int j = 0; j<3; j++) {
				e[i][j] = *a++; // Liste iterieren und Werte übernehmen
			}
		}
	}

	Matrix3 operator*(Matrix3 b) {
		Matrix3 c;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				for (int k = 0; k < 3; k++) {
					c.e[i][j] += e[i][k] * b.e[k][j];
				}
			}
		}
		return c;
	}

	Vector3 operator*(Vector3 v) {
		Vector3 res(0,0,0);
		for (int j = 0; j < 3; j++) {
			double vector_elemet = (j==0) ? v.x : (j==1) ? v.y : v.z;
			res = res + Vector3(e[0][j], e[1][j], e[2][j]) * vector_elemet;
		}
		return res;
	}

	Matrix3 operator*(double s) {
		Matrix3 res;
		for (int i = 0; i<3; i++) {
			for (int j = 0; j<3; j++) {
				res.e[i][j] = e[i][j] * s;
			}
		}
		return res;
	}

	Matrix3 inverse(void) {
		double det = (
			e[0][0]*e[1][1]*e[2][2] + 
			e[0][1]*e[1][2]*e[2][0] + 
			e[0][2]*e[1][0]*e[2][1] -
			e[0][1]*e[1][0]*e[2][2] -
			e[0][2]*e[1][1]*e[2][0] -
			e[0][0]*e[1][2]*e[2][1]
		);
		// cout << "det: " << det << endl; // Determinante ausgeben
		if (det == 0.0) throw runtime_error("Die Matrix ist singulär, bzw. nicht-invertierbar."); // Inverse undefiniert für det = 0;
		return Matrix3(
			{
				e[1][1]*e[2][2] - e[1][2]*e[2][1], 
				e[0][2]*e[2][1] - e[0][1]*e[2][2], 
				e[0][1]*e[1][2] - e[0][2]*e[1][1],

				e[1][2]*e[2][0] - e[1][0]*e[2][2], 
				e[0][0]*e[2][2] - e[0][2]*e[2][0], 
				e[0][2]*e[1][0] - e[0][0]*e[1][2],

				e[1][0]*e[2][1] - e[1][1]*e[2][0], 
				e[0][1]*e[2][0] - e[0][0]*e[2][1], 
				e[0][0]*e[1][1] - e[0][1]*e[1][0]
			}
		) * (1.0/det);
	}
};

atomic<bool> running = true;

void handle_sigint(int) {
	running = false;
}

bool input_available() { // 
	struct timeval tv = {0, 0}; // timeout für select
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	return select(STDIN_FILENO +1, &fds, NULL, NULL, &tv);
}

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
	void dither_bayer(int matrix_size) {
		const uint8_t bayer16[16][16] = { // Die Bayer-Matrix wird für das "ordered-dithering" verwendet.
			{  0, 192,  48, 240,  12, 204,  60, 252,   3, 195,  51, 243,  15, 207,  63, 255 },
			{ 128,  64, 176, 112, 140,  76, 188, 124, 131,  67, 179, 115, 143,  79, 191, 127 },
			{  32, 224,  16, 208,  44, 236,  28, 220,  35, 227,  19, 211,  47, 239,  31, 223 },
			{ 160,  96, 144,  80, 172, 108, 156,  92, 163,  99, 147,  83, 175, 111, 159,  95 },
			{   8, 200,  56, 248,   4, 196,  52, 244,  11, 203,  59, 251,   7, 199,  55, 247 },
			{ 136,  72, 184, 120, 132,  68, 180, 116, 139,  75, 187, 123, 135,  71, 183, 119 },
			{  40, 232,  24, 216,  36, 228,  20, 212,  43, 235,  27, 219,  39, 231,  23, 215 },
			{ 168, 104, 152,  88, 164, 100, 148,  84, 171, 107, 155,  91, 167, 103, 151,  87 },
			{   2, 194,  50, 242,  14, 206,  62, 254,   1, 193,  49, 241,  13, 205,  61, 253 },
			{ 130,  66, 178, 114, 142,  78, 190, 126, 129,  65, 177, 113, 141,  77, 189, 125 },
			{  34, 226,  18, 210,  46, 238,  30, 222,  33, 225,  17, 209,  45, 237,  29, 221 },
			{ 162,  98, 146,  82, 174, 110, 158,  94, 161,  97, 145,  81, 173, 109, 157,  93 },
			{  10, 202,  58, 250,   6, 198,  54, 246,   9, 201,  57, 249,   5, 197,  53, 245 },
			{ 138,  74, 186, 122, 134,  70, 182, 118, 137,  73, 185, 121, 133,  69, 181, 117 },
			{  42, 234,  26, 218,  38, 230,  22, 214,  41, 233,  25, 217,  37, 229,  21, 213 },
			{ 170, 106, 154,  90, 166, 102, 150,  86, 169, 105, 153,  89, 165, 101, 149,  85 }
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
		const uint8_t bayer4[4][4] = { // Die Bayer-Matrix wird für das "ordered-dithering" verwendet.
			{   0, 192,  48, 240,},
			{ 128,  64, 176, 112,},
			{  32, 224,  16, 208,},
			{ 160,  96, 144,  80,}
		};
		const uint8_t bayer2[2][2] = { // Die Bayer-Matrix wird für das "ordered-dithering" verwendet.
			{   0, 192},
			{ 128,  64}
		};
		const uint8_t *matrix = NULL;

		switch (matrix_size) {
		case 2:
			matrix = &bayer2[0][0];
			break;
		case 4:
			matrix = &bayer4[0][0];
			break;
		case 8:
			matrix = &bayer8[0][0];
			break;
		case 16:
			matrix = &bayer16[0][0];
			break;
		default:
			throw invalid_argument("no bayer-matrix of this size");
			return;
		}

		for (size_t i = 0; i < rows; i++) {
			for (size_t j = 0; j < cols; j++) {
				uint8_t threshold = matrix[(i % matrix_size) * matrix_size + (j % matrix_size)];
				buffer[i][j] = threshold < buffer[i][j];
			}
		}
	}
	void print_pixels() {
		//cout << "-----begin half-block print-----" << endl;
		for (size_t i = 0; i < (rows / 2); i++) {
			for (size_t j = 0; j < (cols); j++) {
				if (buffer[i * 2][j] && buffer[(i * 2) + 1][j]) printf("█");
				else if (buffer[i * 2][j]) printf("▀");
				else if (buffer[(i * 2) + 1][j]) printf("▄");
				else printf(" ");
			} std::cout << endl;
		}
		if (rows % 2) { // ggf. ungerade letzte Zeile
			for (size_t j = 0; j < (cols); j++) {
				if (buffer[rows - 1][j]) printf("▀");
				else printf(" ");
			} std::cout << endl;
		}
		//cout << "-----end half-block print-----" << endl;
	}
	void print_values() {
		//cout << "-----begin value print-----" << endl;
		for (size_t i = 0; i < rows; i++) {
			for (size_t j = 0; j < cols; j++) {
				printf("%3d ", buffer[i][j]);
			} std::cout << endl;
		}
		//cout << "-----end value print-----" << endl;
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
	Matrix3 basis; // Normiert
	size_t buffer_rows;
	size_t buffer_cols;
	double screen_distance;
public:
	pixel_buffer render_sphere(Sphere sphere) {
		Vector3 l = Vector3(1, 1, 0).norm(); // Lichtrichtung
		Vector3 d; // Strahlrichtung
		Vector3 v = position - sphere.get_position(); // Mittelpunkt Sphäre -> Kamera
		double r = sphere.get_radius(); // Radius
		double y_offset = double(buffer_cols) / 2.0;
		double z_offset = double(buffer_rows) / 2.0;

		pixel_buffer buffer(buffer_rows, vector<uint8_t>(buffer_cols));

		for (size_t i = 0; i < buffer_rows; i++) {
			for (size_t j = 0; j < buffer_cols; j++) {
				d = basis * Vector3(screen_distance, j + 0.5 - y_offset, i + 0.5 - z_offset); // Basiswechsel der Bildpunkte in globale Koordinaten
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
					double brightness = 255*max(l.dot(n), 0.0);
					buffer[i][j] = brightness;
				}
			}
		}
		return buffer;
	}
	Vector3 get_position() { return position; }
	void set_position(Vector3 new_position) { position = new_position; }
	void move_globally(Vector3 delta_position) {set_position(position + delta_position);}
	void move_locally(Vector3 delta_position) {set_position(position + basis * delta_position);}
	Camera(Vector3 position, Matrix3 basis, size_t buffer_rows, size_t buffer_cols, double screen_distance)
		:position(position), basis(basis), buffer_rows(buffer_rows), buffer_cols(buffer_cols), screen_distance(screen_distance) {};
	void roll(double radians) {

	};
	void pitch(double radians) {

	};
	void yaw(double radians){

	};
};


int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				// Flags können hier gelesen werden
			default:
				throw invalid_argument("invalid flag");
				return EXIT_FAILURE;
			}
		}
		else {
			throw invalid_argument("invalid argument");
			return EXIT_FAILURE;
		}
	}
	signal(SIGINT, handle_sigint);

	// Variablen für die Schleife definieren
	const chrono::milliseconds target_period(20);
	chrono::nanoseconds deltatime;
	Screen screen(SCREENSIZE_ROWS, SCREENSIZE_COLS);
	//Camera camera(Vector3(160.0, 0.0, 0.0), Basis3(Vector3(-1,0,0).norm(), Vector3(0, 1, 0).norm(), Vector3(0, 0, 1).norm()), screen.get_rows(), screen.get_cols(), 100.0);
	Camera camera(Vector3(160.0, 0.0, 0.0), Matrix3({-1,0,0, 0,1,0, 0,0,1}), screen.get_rows(), screen.get_cols(), 100.0);
	Sphere sphere(Vector3(0,0,0), 50.0);
	Vector3 camera_initial_position = camera.get_position();
	double omega = 0.000000001; // Kreisfrequenz für testzwecke

	/* Tests zur einfachen Ausführung
	Matrix3 a({41,12,3,8,2,6,7,2,10});
	Matrix3 b = a.inverse();
	Matrix3 c = a*b;

	cout << "original ↓" << endl;
	for (int i = 0; i<3; i++) {
		for (int j = 0; j<3; j++) {
			cout << a.e[i][j] << " ";
		}
		cout << endl;
	}
	cout << "inverse ↓" << endl;
	for (int i = 0; i<3; i++) {
		for (int j = 0; j<3; j++) {
			cout << b.e[i][j] << " ";
		}
		cout << endl;
	}
	cout << "multiplikation ↓" << endl;
	for (int i = 0; i<3; i++) {
		for (int j = 0; j<3; j++) {
			cout << c.e[i][j] << " ";
		}
		cout << endl;
	}
	//*/

	///* Main loop
	std::cout << "\033[2J\033[?25l"; // Bildschirm leeren, Cursor unsichtbar
	while (running) {
		chrono::steady_clock::time_point start_time = chrono::steady_clock::now();
		std::cout << "\033[H"; // Cursor auf 0,0

		// ----- Schleife beginnt -----

		// Ggf. Tastatureingabe (durch terminal beschränkung nur eine Taste gleichzeitig)
		if (input_available()) {
            string input;
            getline(std::cin, input);
			// std::cout << input [0];
			switch (input[0])
			{
			case 'w': // Nicken herunter
				
				break;
			case 's': // Nicken hoch
				
				break;
			case 'a': // Gieren links
				
				break;
			case 'd': // Gieren rechts
				
				break;
			case 'q': // Rollen CCW
				
				break;
			case 'e': // Rollen CW
				
				break;
			case 'h': // Trans. links
				camera.move_locally(Vector3(0,(-1)*TRANSLATION_UNIT,0));
				break;
			case 'j': // Trans. unten
				camera.move_locally(Vector3(TRANSLATION_UNIT,0,0));
				break;
			case 'k': // Trans. oben
				camera.move_locally(Vector3((-1)*TRANSLATION_UNIT,0,0));
				break;
			case 'l': // Trans. rechts
				camera.move_locally(Vector3(0,TRANSLATION_UNIT,0));
				break;
			case 'i': // Trans. vor
				camera.move_locally(Vector3(0,0,TRANSLATION_UNIT));
				break;
			case 'm': // Trans. zurück
				camera.move_locally(Vector3(0,0,(-1)*TRANSLATION_UNIT));
				break;

			default:
				break;
			}
        }


		/*
		double offset = 100.0 * sin(chrono::steady_clock::now().time_since_epoch().count() * omega);
		// cout << "timedelta: " << deltatime << "\t" << endl;
		camera.set_position(camera_initial_position + Vector3(1, 0, 0) * offset);
		screen.set_buffer(camera.render_sphere(sphere));
		screen.dither_bayer(8);
		screen.print_pixels();
		//*/

		// ----- Schleife endet -----

		chrono::steady_clock::time_point end_time = chrono::steady_clock::now();
		deltatime = end_time - start_time;
		std::this_thread::sleep_for(target_period - deltatime);
	}
	std::cout << "\nBYE!^-^\033[?25h" << endl;; // Bildschirm leeren, Cursor unsichtbar
	//*/
	return EXIT_SUCCESS;
}
