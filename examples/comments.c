// Line comments are ignored by the lexer.
int helper(int value)
{
    /*
       Block comments can span lines and appear between statements.
    */
    return value + 3;
}

int main()
{
    int total = 4; // trailing comments are ignored too

    total += helper(5);
    /*
       Operators inside comments should not be tokenized:
       && || += <<= (unsigned short)
    */
    return total;
}
