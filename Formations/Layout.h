// THE SHAPES, shared by the plugin and formtest.exe so what is looked at is what
// ships. The host defines Vec3 { float x, y, z; } before including.
//
// A formation is N slots on the ground, laid out from the drag: A is where the
// drag began, B where it ended. `along` runs A->B, `across` is its right-hand
// perpendicular. Bannerlord's shapes, in Kenshi's terms (boot 9, owner: "tight
// line, loose line and column are all the same thing... bannerlord has a
// tight/loose toggle" - so four shapes, and the spacing carries the toggle):
//   0 line          evenly spaced along the drag (the drag stretches it, never squeezes below the spacing)
//   1 circle        the drag is the diameter
//   2 rectangle     the drag is the front rank; ranks fill behind it (the left of the drag direction)
//   3 wedge         the point at the far end of the drag, ranks of 1, 2, 3... behind it
// (the column went in boot 10 - the march is always a column, and standing in one is a line)
#pragma once
static int Layout(int shape, int n, const Vec3& A, const Vec3& B, float spacing, Vec3* slots)
{
    float ax = B.x - A.x, az = B.z - A.z, L = sqrtf(ax * ax + az * az);
    float ux, uz;                                      // along
    if (L < 1e-3f) { ux = 0; uz = -1; } else { ux = ax / L; uz = az / L; }
    float rx = -uz, rz = ux;                           // across (right)
    float cx = (A.x + B.x) * 0.5f, cz = (A.z + B.z) * 0.5f;
    int k = 0;
    auto put = [&](float ox, float oz) { if (k < n) { slots[k].x = cx + ux * ox + rx * oz; slots[k].z = cz + uz * ox + rz * oz; slots[k].y = 0; k++; } };
    switch (shape)
    {
    case 0:
    {
        float width = L > spacing * (n - 1) ? L : spacing * (n - 1);
        for (int i = 0; i < n; i++) put(n > 1 ? -width * 0.5f + width * i / (n - 1) : 0, 0);
        break;
    }
    case 1:
    {
        float r = L * 0.5f; float minR = spacing * n / 6.2831853f; if (r < minR) r = minR;
        for (int i = 0; i < n; i++) { float th = 6.2831853f * i / n; put(cosf(th) * r, sinf(th) * r); }
        break;
    }
    case 2:
    {
        int cols = (int)(L / spacing) + 1; if (cols < 1) cols = 1; if (cols > n) cols = n;
        if (L < 1e-3f) cols = (int)ceilf(sqrtf((float)n));
        int rows = (n + cols - 1) / cols;
        for (int i = 0; i < n; i++)
        {
            int r = i / cols, c = i % cols;
            int inRow = (r == rows - 1) ? n - r * cols : cols;
            float w = spacing * (inRow - 1);
            put(inRow > 1 ? -w * 0.5f + spacing * c : 0, -spacing * r);
        }
        break;
    }
    case 3:
    {
        int i = 0, row = 0;
        float tip = L > 1e-3f ? L * 0.5f : 0;
        while (i < n)
        {
            int inRow = row + 1; if (i + inRow > n) inRow = n - i;
            float w = spacing * (inRow - 1);
            for (int c = 0; c < inRow; c++) put(tip - spacing * row, inRow > 1 ? -w * 0.5f + spacing * c : 0);
            i += inRow; row++;
        }
        break;
    }
    }
    return k;
}
