#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VASECOUNT 4

uint8_t vases[VASECOUNT];
/*
 * idx 0 -> how many vases with 0 flowers
 * idx 1 -> how many vases with 1 flowers
 * idx n -> how many vases with n flowers
 */
uint8_t vasecounts[VASECOUNT];

/*!
 * @brief: Fill a vase at @index
 */
static void fill_vase(int index)
{
    /* Count the fuckers */
    for (int i = 0; i < VASECOUNT; i++)
        vasecounts[vases[i]]++;

    /* Put the things in */
    vases[index] = vasecounts[index];
}

static void dump_vases()
{
    for (int i = 0; i < VASECOUNT; i++)
        printf("Vase %d has %d flowers in it\n", i, vases[i]);

    printf("\n");

    for (int i = 0; i < VASECOUNT; i++)
        printf("There are %d vases with %d flowers in them\n", vasecounts[i], i);
}

/*!
 * @brief: Quick program that solves a vase-flower problem
 *
 * There are VASECOUNT vases:
 * Vase 1 has as many flowers as there are empty vases
 * Vase 2 has as many flowers as there are vases with one flower
 * Vase 3 has as many flowers as there are vases with two flowers
 * Vase VASECOUNT has as many flowers as there are vases with three flowers
 *
 * How are the vases filled, so that the rules above are satisfied?
 */
int main()
{
    memset(vases, 0, sizeof(vases));

    for (uint32_t i = 0; i < 999; i++) {

        for (int j = VASECOUNT + 1; j; j--) {
            memset(vasecounts, 0, sizeof(vasecounts));

            fill_vase(j - 1);
        }
    }

    dump_vases();
    return 0;
}
