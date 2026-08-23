/* The statement after a return cannot run; a warning, not an error. */
int main()
{
    return 1;
    return 2;
}
