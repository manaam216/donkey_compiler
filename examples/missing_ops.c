int main()
{
    int x = 2;
    int y = 5;
    int z = 0;

    z += x << 3;
    z -= y >> 1;
    z *= 2;
    z /= 7;
    z %= 5;
    z |= 8;
    z &= 15;
    z ^= 3;
    z += sizeof(int);
    z += sizeof(x + y);
    z += (int)2;
    z += x++ + ++x;
    z += y-- + --y;
    z += (x > y ? 10 : 20);
    z += (x = 1, y = 2, x + y);

    return z;
}
