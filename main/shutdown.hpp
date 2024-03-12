
void task_shutDownDetection(void *pvParameter);

// read last axis position in steps from nvs
// returns -1 when reading from nvs failed
int nvsReadLastAxisPosSteps();