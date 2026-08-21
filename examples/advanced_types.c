struct Point {
    int x;
    int y;
};

int first(int values[4])
{
    return values[0];
}

int main()
{
    int values[4] = {7, 9, 11, 13};
    int *start = values;
    int *end = values + 3;
    char letter = 'A';
    char newline = '\n';
    char *text = "Hi";
    struct Point p;

    p.x = first(values);
    p.y = end - start;
    return p.x + p.y + letter + newline + (char)text[0];
}
