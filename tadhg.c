#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Enum for scheduling types
enum SchedulingType {
    NORMAL,
    RR,
    FIFO
};

// Define the struct to hold the data
struct ProcessData {
    int pid;
    enum SchedulingType policy;
    int priority;
    int execution_time;
    int cpu_affinity;
};

// Function to convert a string to the corresponding enum value
enum SchedulingType get_scheduling_type(const char *policy_str) {
    if (strcmp(policy_str, "NORMAL") == 0) {
        return NORMAL;
    } else if (strcmp(policy_str, "RR") == 0) {
        return RR;
    } else if (strcmp(policy_str, "FIFO") == 0) {
        return FIFO;
    } else {
        // Default to NORMAL if the string doesn't match any known type
        return NORMAL;
    }
}

// Function to parse a CSV line and store the data in the struct
int parse_csv_line(FILE *file, struct ProcessData *process_data) {
    char policy_str[7]; // Temporary buffer for the policy string

    int result = fscanf(file, "%d,%6[^,],%d,%d,%d",
                        &process_data->pid,
                        policy_str,
                        &process_data->priority,
                        &process_data->execution_time,
                        &process_data->cpu_affinity);

    if (result == 0) {
        // fscanf returns 0 when it fails to match any items
        return 0;
    } else if (result != 5) {
        // Parsing error
        return -1;
    }

    // Convert the policy string to the corresponding enum value
    process_data->policy = get_scheduling_type(policy_str);

    return 1; // Success
}

int main() {
    FILE *file = fopen("pcb_data.csv", "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    struct ProcessData process_data;

    while (1) {
        int result = parse_csv_line(file, &process_data);

        if (result == 1) {
            // Successfully parsed a line, you can use process_data here
            printf("PID: %d, Policy: %d, Priority: %d, Execution Time: %d, CPU Affinity: %d\n",
                   process_data.pid, process_data.policy, process_data.priority,
                   process_data.execution_time, process_data.cpu_affinity);
        } else if (result == 0) {
            // End of file or invalid input line
            break;
        } else {
            // Parsing error
            fprintf(stderr, "Error parsing CSV line\n");
            break;
        }
    }

    fclose(file);

    return 0;
}
