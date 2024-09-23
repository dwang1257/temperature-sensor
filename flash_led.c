#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main() {
    struct timespec curr;
    struct timespec prev;
    printf("\n LED Flash Start \n");
    FILE *LEDHandle = NULL;
    FILE *filePointer = fopen("time_diff_file.txt", "w");
    // directory path to the 4th led's brightness file
    const char *LEDBrightness = "/sys/class/leds/beaglebone:green:usr3/brightness";

    // flash the led 1000 times
    for (int i = 0; i <1000; i++) {
        // Open the file
        // Read/Write only if the file pointer is not NULL
        // Always check for this condition to avoid Segmentation fault
        if ((LEDHandle = fopen(LEDBrightness, "r+")) != NULL) {
            // Turn the led on
            fwrite("1", sizeof(char), 1, LEDHandle);
            // Close the led file
            fclose(LEDHandle);
        }
        clock_gettime(CLOCK_MONOTONIC, &prev);
        // sleep for 10^6 microsecond or 1 second
        usleep(500000);
        // Open the file and check for the same condition as above
        if ((LEDHandle = fopen(LEDBrightness, "r+")) != NULL) {
            // Turn the led off
            fwrite("0", sizeof(char), 1, LEDHandle);
            // Close the led file
            fclose(LEDHandle);
        }

        clock_gettime(CLOCK_MONOTONIC, &curr);

        long diff = (curr.tv_sec - prev.tv_sec) * 1000000000L + (curr.tv_nsec - prev.tv_nsec);

        fprintf(filePointer, "%ld\n", diff);

        usleep(500000);
    }
    
    // Close the file after writing
    fclose(filePointer);
    
    printf("\n LED Flash End \n");
}