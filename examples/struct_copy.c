/*
 * Assigning one struct to another copies the whole thing. Before this it
 * copied a single machine word and silently produced the wrong answer.
 */
struct Triple { int x; int y; int z; };

int main()
{
    struct Triple a;
    struct Triple b;

    a.x = 1;
    a.y = 2;
    a.z = 3;

    b.x = 0;
    b.y = 0;
    b.z = 0;

    b = a;

    return b.x * 100 + b.y * 10 + b.z;
}
