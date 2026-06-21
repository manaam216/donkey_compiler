unsigned char global_byte = 300;
char global_signed_byte = (char)255;

unsigned char narrow_return()
{
    return 300;
}

int narrow_argument(unsigned char value)
{
    return value;
}

int main()
{
    unsigned int high = (unsigned int)-1;
    unsigned char counter = 255;
    long negative = -1;
    unsigned int zero = 0;

    counter++;
    return narrow_return()
        + narrow_argument(300)
        + (high > 1) * 10
        + (high >> 31)
        + ((high / 2) > 100) * 20
        + global_byte
        + global_signed_byte
        + counter
        + (negative < zero);
}
