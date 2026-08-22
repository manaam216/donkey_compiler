/*
 * Regression coverage for the sizing bugs Phase 2 fixed.
 *
 * Every type used to occupy a 4-byte slot, so char arrays were indexed by 4
 * and stored with a 4-byte movl. Reading letters[2] fetched the wrong address,
 * and writing letters[0] clobbered letters[1..3].
 */
int main()
{
    char letters[4];
    char *cursor;
    int total;

    letters[0] = 'A';
    letters[1] = 'B';
    letters[2] = 'C';
    letters[3] = 'D';

    /* Wrong index scaling would read past the array here. */
    total = letters[2];

    /* Neighbours must survive each store: 65 + 66 + 67 + 68 = 266. */
    total = total + letters[0] + letters[1] + letters[2] + letters[3];

    /* char* arithmetic advances one byte at a time, not four. */
    cursor = letters;
    cursor = cursor + 2;
    total = total + *cursor;

    /* sizeof measures the array, not a pointer or a padded slot. */
    total = total + sizeof(letters);

    return total;
}
