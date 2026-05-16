int shared = 5;
int zero;

int helper(int amount)
{
    shared += amount;
    zero = shared - 1;
    return zero;
}

int main()
{
    int shared = 2;
    int first = helper(4);

    shared += 3;
    return first + shared + zero;
}
