// FORMTEST - the shapes, looked at before the DLL ships. Prints every shape's
// slots for a squad of N along a drag, as a text picture, so a wedge that points
// sideways is caught here and not in a fight.
//
//   formtest [N] [dragLength]
#define STARTPLUGIN_TEST 1
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
struct Vec3 { float x, y, z; };
// the layout function, lifted verbatim from Formations.cpp by the build (see build.bat)
#include "Layout.h"

int main(int argc, char** argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 7;
    float L = argc > 2 ? (float)atof(argv[2]) : 12.0f;
    const char* names[] = { "Line", "Circle", "Rectangle", "Wedge" };
    Vec3 A = { 0, 0, 0 }, B = { L, 0, 0 };            // the drag runs +x: "along" is +x, "across" is +z
    Vec3 slots[64];
    for (int s = 0; s < 4; s++)
    {
        int k = Layout(s, n, A, B, 2.0f, slots);
        printf("\n%s  (%d slots; drag %.0f long, A at x=0, B at x=%.0f)\n", names[s], k, L, L);
        // a 41x21 picture, 1 cell = 1 unit, centred on the drag's middle
        char pic[21][42];
        for (int y = 0; y < 21; y++) { for (int x = 0; x < 41; x++) pic[y][x] = '.'; pic[y][41] = 0; }
        pic[10][20 - (int)(L / 2)] = 'A'; pic[10][20 + (int)(L / 2)] = 'B';
        for (int i = 0; i < k; i++)
        {
            int px = 20 + (int)floorf(slots[i].x - L / 2 + 0.5f), py = 10 + (int)floorf(slots[i].z + 0.5f);
            if (px >= 0 && px < 41 && py >= 0 && py < 21) pic[py][px] = i < 10 ? '0' + i : 'a' + (i - 10);
        }
        for (int y = 0; y < 21; y++) printf("  %s\n", pic[y]);
    }
    return 0;
}
