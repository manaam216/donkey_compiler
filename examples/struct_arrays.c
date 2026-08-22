/*
 * Array-of-struct and short arrays. Both were broken before types existed:
 * subscripting dropped the struct tag, so pts[0].x failed to compile, and
 * element strides ignored the real element size.
 */
struct Point { int x; int y; };
int main()
{
    struct Point pts[3];
    short halves[4];
    int total;
    pts[0].x = 1; pts[0].y = 2;
    pts[1].x = 10; pts[1].y = 20;
    pts[2].x = 100; pts[2].y = 200;
    halves[0] = 300; halves[1] = 400; halves[2] = 500; halves[3] = 600;
    total = pts[0].x + pts[0].y + pts[1].x + pts[1].y + pts[2].x + pts[2].y;
    total = total + halves[0] + halves[1] + halves[2] + halves[3];
    return total + sizeof(pts) + sizeof(halves);
}
