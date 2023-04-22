

void stepper_init();

void stepper_setTargetSteps(int steps);

void stepper_toggleDirection();

//control stepper without timer (software)
void task_stepperCtrlSw(void *pvParameter);
void stepperSw_setTargetSteps(uint64_t target);

//typedef struct
//{
//    uint8_t stepPin;           /** step signal pin */
//    uint8_t dirPin;            /** dir signal pin */
//    timer_group_t timer_group; /** timer group, useful if we are controlling more than 2 steppers */
//    timer_idx_t timer_idx;     /** timer index, useful if we are controlling 2steppers */
//    float stepAngle;           /** one step angle in degrees (usually 1.8deg), used in steps per rotation calculation */
//} stepper_config_t;
//
//typedef struct
//{
//    uint32_t stepInterval = 40000; // step interval in ns/25
//    double targetSpeed = 0;        // target speed in steps/s
//    double currentSpeed = 0;
//    double accInc = 0;
//    double decInc = 0;
//    uint32_t stepCnt = 0;   // step counter
//    uint32_t accEnd;        // when to end acc and start coast
//    uint32_t coastEnd;      // when to end coast and start decel
//    uint32_t stepsToGo = 0; // steps we need to take
//    float speed = 100;      // speed in rad/s
//    float acc = 100;        // acceleration in rad*second^-2
//    float dec = 100;        // decceleration in rad*second^-2
//    uint32_t accSteps = 0;
//    uint32_t decSteps = 0;
//    uint8_t status = DISABLED;
//    bool dir = CW;
//    bool runInfinite = false;
//    uint16_t stepsPerRot;    // steps per one revolution, 360/stepAngle * microstep
//    uint16_t stepsPerMm = 0; /** Steps per one milimiter, if the motor is performing linear movement */
//} ctrl_var_t;

