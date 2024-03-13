
// task that repeatedly checks supply voltage (12V) and saves certain values to nvs in case of it drops below a certain threshold (power off detected)
void task_shutDownDetection(void *pvParameter);

// read last axis position in steps from nvs
// returns -1 when reading from nvs failed
int nvsReadLastAxisPosSteps();