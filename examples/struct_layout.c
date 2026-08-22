/*
 * Struct field offsets used to be field_index * 4, which ignored both the
 * real field sizes and the padding needed to keep each field aligned.
 */
struct Mixed {
    char tag;
    int value;
    char flag;
};

struct Packed {
    char a;
    char b;
    char c;
};

int main()
{
    struct Mixed m;
    struct Packed p;
    int total;

    m.tag = 3;
    m.value = 1000;
    m.flag = 7;

    p.a = 1;
    p.b = 2;
    p.c = 3;

    /* Overlapping fields would corrupt these values. */
    total = m.tag + m.value + m.flag;
    total = total + p.a + p.b + p.c;

    /* 1 + 3 padding + 4 + 1 + 3 tail padding = 12; three chars = 3. */
    total = total + sizeof(m) + sizeof(p);

    return total;
}
