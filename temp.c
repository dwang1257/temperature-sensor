#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>

// Function to configure pin in given mode
void setup_pin(char* pin_number, char* mode){
    if (!vfork()) {
        int ret = execl("/usr/bin/config-pin", "config-pin", pin_number, mode, NULL);
        if (ret < 0) {
            printf("Failed to configure pin in PWM mode.\n");
            exit(-1);
        }
    }
}

// Function to set up GPIO pin as input
void configure_gpio_input(int gpio_number){
    char gpio_num[10];
    sprintf(gpio_num, "export%d", gpio_number);
    const char* GPIOExport = "/sys/class/gpio/export";
    FILE* fp = fopen(GPIOExport, "w");
    fwrite(gpio_num, sizeof(char), sizeof(gpio_num), fp);
    fclose(fp);
    char GPIODirection[40];
    sprintf(GPIODirection, "/sys/class/gpio/gpio%d/direction", gpio_number);
    fp = fopen(GPIODirection, "w");
    fwrite("in", sizeof(char), 2, fp);
    fclose(fp);
}

// Function to configure interrupt on the given GPIO pin
void configure_interrupt(int gpio_number){
    configure_gpio_input(gpio_number);
    char InterruptEdge[40];
    sprintf(InterruptEdge, "/sys/class/gpio/gpio%d/edge", gpio_number);
    FILE* fp = fopen(InterruptEdge, "w");
    fwrite("falling", sizeof(char), 7, fp);
    fclose(fp);
}

// Function to set PWM duty cycle
void set_pwm_duty_cycle(char* pwmchip, char* channel, char* duty_cycle){
    char PWMDutyCycle[60];
    sprintf(PWMDutyCycle, "/sys/class/pwm/%s/pwm-4:%s/duty_cycle", pwmchip, channel);
    FILE* fp = fopen(PWMDutyCycle, "w");
    fwrite(duty_cycle, sizeof(char), strlen(duty_cycle), fp);
    fclose(fp);
}

// Function to set PWM period
void set_pwm_period(char* pwmchip, char* channel, char* period){
    long duty_cycle_int, period_int;
    char PWMDutyCycle[60], duty_cycle_str[20];
    sprintf(PWMDutyCycle, "/sys/class/pwm/%s/pwm-4:%s/duty_cycle", pwmchip, channel);
    FILE* fp = fopen(PWMDutyCycle, "r");
    fscanf(fp, "%ld", &duty_cycle_int);
    fclose(fp);
    period_int = atol(period);
    if (duty_cycle_int >= period_int){
        duty_cycle_int = period_int / 2;
        sprintf(duty_cycle_str, "%ld", duty_cycle_int);
        set_pwm_duty_cycle(pwmchip, channel, duty_cycle_str);
    }
    char PWMPeriod[60];
    sprintf(PWMPeriod, "/sys/class/pwm/%s/pwm-4:%s/period", pwmchip, channel);
    fp = fopen(PWMPeriod, "w");
    fwrite(period, sizeof(char), strlen(period), fp);
    fclose(fp);
}

// Function to start PWM
void start_pwm(char* pin_number, char* pwmchip, char* channel, char* period, char* duty_cycle){
    FILE* fp;
    char PWMExport[40];
    sprintf(PWMExport, "/sys/class/pwm/%s/export", pwmchip);
    fp = fopen(PWMExport, "w");
    fwrite(channel, sizeof(char), sizeof(channel), fp);
    fclose(fp);
    set_pwm_period(pwmchip, channel, period);
    set_pwm_duty_cycle(pwmchip, channel, duty_cycle);
    char PWMEnable[40];
    sprintf(PWMEnable, "/sys/class/pwm/%s/pwm-4:%s/enable", pwmchip, channel);
    fp = fopen(PWMEnable, "w");
    fwrite("1", sizeof(char), 1, fp);
    fclose(fp);
}

// Function to stop PWM
void stop_pwm(char* pin_number, char* pwmchip, char* channel){
    char PWMDisable[40];
    sprintf(PWMDisable, "/sys/class/pwm/%s/pwm-4:%s/enable", pwmchip, channel);
    FILE* fp = fopen(PWMDisable, "w");
    fwrite("0", sizeof(char), 1, fp);
    fclose(fp);
}

// Function for thread execution
void* execute_thread(void *var){
    int* input = (int *) var;
    sleep(1);
    printf("Received %d inside thread argument.\n", *input);
    return NULL;
}

// Function to read digital value from ADC interface
const char AIN_DEV[] = "/sys/bus/iio/devices/iio:device0/in_voltage1_raw";

// Function to convert Celsius to Fahrenheit
double Celsius_to_Fahrenheit(double celsius){
    return (celsius * 9.0 / 5.0) + 32.0;
}

// Function to extract temperature from TMP36 data
double temperature(int value){
    double millivolts = (value / 4096.0) * 1800;
    double temp_celsius = (millivolts - 500.0) / 10.0;
    return temp_celsius;
}

// Structure for temperature timestamp
typedef struct {
    struct timespec timestamp;
    double temp_c;
} temp_reading;

int count = 0;
temp_reading buffer[10];
pthread_mutex_t mutex;

// Function for input thread
void *input_thread(void *arg){
    struct epoll_event event;
    int gpio_fd, epoll_fd, temp_fd;
    gpio_fd = open("/sys/class/gpio/gpio68/value", O_RDONLY);
    temp_fd = -1;
    epoll_fd = epoll_create(1);
    char temp[16] = {0};
    if (gpio_fd == -1 || epoll_fd == -1) return (void *)0x0;

    event.events = EPOLLIN | EPOLLET;
    event.data.fd = gpio_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, gpio_fd, &event) == -1) return (void *)0x0;

    struct epoll_event event_wait;
    struct timespec tm;

    for (count = 0; count <= 50; count++){
        if (epoll_wait(epoll_fd, &event_wait, 1, -1) == -1) return (void *)0x0;

        pthread_mutex_lock(&mutex);
        clock_gettime(CLOCK_MONOTONIC, &tm);
        buffer[count % 10].timestamp = tm;
        if ((temp_fd = open(AIN_DEV, O_RDONLY)) == -1) return (void *)0x0;
        read(temp_fd, temp, sizeof(temp));
        int temp_value = atoi(temp);
        buffer[count % 10].temp_c = temperature(temp_value);
        close(temp_fd);
        pthread_mutex_unlock(&mutex);
    }
    close(gpio_fd);
    return (void *)0x0;
}

// Function for output thread
void *output_thread(void *arg){
    int write_index = 0;
    FILE *fp = fopen("sensor_data.txt", "wb");
    if (fp == NULL) {
        perror("Error creating file");
        pthread_exit(NULL);
    }

    while (count <= 50) {
        while (write_index != count) {
            pthread_mutex_lock(&mutex);
            fprintf(fp, "%d%09d %f\n", buffer[write_index % 10].timestamp.tv_sec, buffer[write_index % 10].timestamp.tv_nsec, buffer[write_index % 10].temp_c);
            printf("%d%09d %f\n", buffer[write_index % 10].timestamp.tv_sec, buffer[write_index % 10].timestamp.tv_nsec, buffer[write_index % 10].temp_c);
            pthread_mutex_unlock(&mutex);
            write_index++;
        }
    }
    fclose(fp);
}

int main(int argc, const char *argv[]) {
    int i;
    pthread_t threads[2];

    if ((pthread_mutex_init(&mutex, NULL)) != 0) return 1;

    pthread_create(&threads[0], NULL, input_thread, NULL);
    pthread_create(&threads[1], NULL, output_thread, NULL);

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    pthread_mutex_destroy(&mutex);
    return 0;
}
