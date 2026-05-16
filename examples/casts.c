int main()
{
    int x = 260;
    int y = 65535;
    int total = 0;

    total += (char)x;
    total += (signed char)x;
    total += (unsigned char)x;
    total += (short)y;
    total += (signed short int)y;
    total += ((unsigned short)y) % 7;
    total += (int)3;
    total += (long)2;
    total += (unsigned int)1;
    total += (unsigned long)1;
    total += sizeof(char);
    total += sizeof(short);
    total += sizeof(int);
    total += sizeof(unsigned long);

    return total;
}
