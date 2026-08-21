/* Regression: lengths above 256 used to overflow globals[].values[256]. */
int big[400] = {1};

int main()
{
    return big[0];
}
