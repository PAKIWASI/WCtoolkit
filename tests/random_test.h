#ifndef RANDOM_TEST_H
#define RANDOM_TEST_H

#include "random.h"


int random_test_1(void)
{
    pcg32_rand_seed(1, 3);

    printf("Normal rand\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%d\n", pcg32_rand());
    }

    printf("bounded rand\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%d\n", pcg32_rand_bounded(100));
    }

    printf("\n Time Seed\n");
    pcg32_rand_seed_time();

    printf("Normal rand\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%d\n", pcg32_rand());
    }

    printf("bounded rand\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%d\n", pcg32_rand_bounded(100));
    }



    printf("Normal rand\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%d\n", pcg32_rand());
    }

    printf("bounded rand\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%d\n", pcg32_rand_bounded(100));
    }

    return 0;
}

int random_test_2(void)
{
    pcg32_rand_seed(1, 1);

    printf("Normal rand float[0, 1]\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%f\n", pcg32_rand_float());
    }

    printf("Normal rand double[0, 1]\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%.15f\n", pcg32_rand_double());
    }

    printf("rand float range\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%f\n", pcg32_rand_float_range(1, 3));
    }


    return 0;
}


// testing gaussian
int random_test_3(void)
{
    pcg32_rand_seed(1, 1);
    
    printf("normal gaussian\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%f\n", pcg32_rand_gaussian());
    }

    printf("gaussian custom [-1, 1]\n");
    for (u64 i = 0; i < 10; i++) {
        printf("%f\n", pcg32_rand_gaussian_custom(0, 1));
    }


    return 0;
}

int random_test_4(void)
{
    pcg32_rand_seed(1, 1);


    // Visualize gaussian distribution by counting values in buckets
    u32 range[200] = {0};   // buckets for range [-1, 1]

    for (u64 i = 0; i < 100000; i++) {  // Need many samples to see distribution
        float g = pcg32_rand_gaussian_custom(0, 1);
        
        // Map [-1, 1] to bucket indices [0, 199]
        // -1.0 maps to bucket 0
        //  0.0 maps to bucket 100
        // +1.0 maps to bucket 199
        int index = (int)((g + 1.0f) * 100.0f);
        
        // Clamp to valid range (values outside [-1, 1] do occur)
        if (index < 0) { index = 0; }
        if (index >= 200) { index = 199; }
        
        range[index]++;
    }

    // Print the distribution
    printf("Gaussian Distribution Visualization\n");
    printf("Range [-1.0, 1.0] divided into 200 buckets\n\n");

    for (int i = 0; i < 200; i++) {
        float bucket_center = -1.0f + ((float)i / 100.0f);
        printf("%+.2f: ", bucket_center);
        
        // Print bar (scale down for display)
        int bar_length = (int)range[i] / 50;  // Adjust scaling as needed
        for (int j = 0; j < bar_length; j++) {
            printf("█");
        }
        printf(" (%u)\n", range[i]);
    }

    return 0;
}

int random_test_5(void)
{
    /* -lm
    const int N = 100000;
    float sum = 0, sum_sq = 0;
    int in_1sigma = 0, in_2sigma = 0;
    
    for (int i = 0; i < N; i++) {
        float x = pcg32_rand_gaussian();
        sum += x;
        sum_sq += x * x;
        if (fabsf(x) <= 1.0f) in_1sigma++;
        if (fabsf(x) <= 2.0f) in_2sigma++;
    }
    
    float mean = sum / N;
    float variance = (sum_sq / N) - (mean * mean);
    float stddev = sqrtf(variance);
    
    printf("Mean: %f (should be ~0)\n", mean);
    printf("StdDev: %f (should be ~1)\n", stddev);
    printf("Within 1σ: %.1f%% (should be ~68%%)\n", 100.0f * in_1sigma / N);
    printf("Within 2σ: %.1f%% (should be ~95%%)\n", 100.0f * in_2sigma / N);

    */
    return 0;
}



#endif // RANDOM_TEST_H
