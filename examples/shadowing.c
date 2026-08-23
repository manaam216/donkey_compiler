/*
 * Variable shadowing. Rejected before Phase 3, because the code generator
 * identified variables by name and two 'value's would have collided. Each
 * declaration now has its own symbol, so they have distinct storage.
 *
 * The two inner blocks are disjoint, so they share a stack slot: this needs
 * 12 bytes of frame, not 16.
 */
int main()
{
    int value;
    int total;
    value = 1;
    total = 0;
    {
        int value;
        value = 20;
        total = total + value;
    }
    total = total + value;
    {
        int value;
        value = 300;
        total = total + value;
    }
    return total + value;
}
