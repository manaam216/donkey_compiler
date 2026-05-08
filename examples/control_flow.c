int choose(int x)
{
    if (x > 5) {
        return x;
    } else {
        return 5;
    }
}

int main()
{
    int i = 0;
    int total = 0;

    while (i < 5) {
        i = i + 1;
        if (i == 3) {
            continue;
        }
        total = total + i;
    }

    for (int j = 0; j < 5; j = j + 1) {
        if (j == 4) {
            break;
        }
        total = total + 1;
    }

    return choose(total);
}
