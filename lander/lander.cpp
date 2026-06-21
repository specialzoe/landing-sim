// Ziel: Steuerung eines Landefahrzeugs aus einem Orbit um eine Kugel heraus.
// Gemachte Vereinfacherungen: ###
//
// Kompilieren am besten über WSL oder in Linux direkt

#define SCREENSIZE_ROWS 256
#define SCREENSIZE_COLS 256

#include <iostream>
#include <vector>
#include <math.h>

typedef unsigned char uint8_t;
using namespace std;

int main(void) {
	// Das Feld "pixel_space" enthält Pixel mit einem 8-Bit Helligkeitswert.
	vector<vector<uint8_t>> pixel_space(SCREENSIZE_ROWS, vector<uint8_t>(SCREENSIZE_COLS));

	// Die Bayer-Matrix wird für das "ordered-dithering" verwendet.
	uint8_t bayer8[8][8] =
	{
		{   0, 192,  48, 240,   6, 198,  54, 246 },
		{ 128,  64, 176, 112, 134,  70, 182, 118 },
		{  32, 224,  16, 208,  38, 230,  22, 214 },
		{ 160,  96, 144,  80, 166, 102, 150,  86 },
		{   8, 200,  56, 248,   2, 194,  50, 238 },
		{ 136,  72, 184, 120, 130,  66, 178, 114 },
		{  40, 232,  24, 216,  34, 226,  18, 210 },
		{ 168, 104, 152,  88, 162,  98, 146,  82 }
	};

	// Initialisieren mit Testmuster
	for (int i = 0; i < SCREENSIZE_ROWS; i++){
		for (int j = 0; j < SCREENSIZE_COLS; j++) {
			pixel_space[i][j] = int(((i * j)/128) % 256);
		}
	}

	// Textausgabe des Musters
	//cout << "Helligkeitswerte" << endl;
	//for (int i = 0; i < SCREENSIZE_ROWS; i++) {
	//	for (int j = 0; j < SCREENSIZE_COLS; j++) {
	//		printf("%3d ", pixel_space[i][j]);
	//	}
	//	cout << endl;
	//}
	//cout << endl;

	// Dither des Musters
	for (int i = 0; i < SCREENSIZE_ROWS; i++) {
		for (int j = 0; j < SCREENSIZE_COLS; j++) {
			pixel_space[i][j] = bayer8[i % 8][j % 8] < pixel_space[i][j];
		}
	}

	// Ausgabe des Musters
	cout << "Dither" << endl;
	for (int i = 0; i < SCREENSIZE_ROWS; i++) {
		for (int j = 0; j < SCREENSIZE_COLS; j++) {
			if (pixel_space[i][j]) printf("█");
			else printf(" ");
		}
		cout << endl;
	}
	cout << endl;

	return EXIT_SUCCESS;
}