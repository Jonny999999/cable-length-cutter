#include "DendoStepper.h"
#include "esp_log.h"

#define STEP_DEBUG

#ifdef STEP_DEBUG
#define STEP_LOGI(...) ESP_LOGI(__VA_ARGS__)
#define STEP_LOGW(...) ESP_LOGW(__VA_ARGS__)
#define STEP_LOGE(...) ESP_LOGE(__VA_ARGS__)
#else
#define STEP_LOGI(...) while (0)
#define STEP_LOGW(...) while (0)
#define STEP_LOGE(...) ESP_LOGE(__VA_ARGS__)
#endif

bool state = 0;

// PUBLIC definitions

DendoStepper::DendoStepper()
{
}

void DendoStepper::config(DendoStepper_config_t *config)
{
    memcpy(&conf, config, sizeof(conf));
}

void DendoStepper::disableMotor()
{
    setEn(true);
    STEP_LOGI("DendoStepper", "Disabled");
    ctrl.status = DISABLED;
}

void DendoStepper::enableMotor()
{
    setEn(false);
    ctrl.status = IDLE;
    STEP_LOGI("DendoStepper", "Enabled");
    timerStarted = 0;
}

void DendoStepper::init(uint8_t stepP, uint8_t dirP, uint8_t enP, timer_group_t group, timer_idx_t index, microStepping_t microstepping = MICROSTEP_1, uint16_t stepsPerRot = 200)
{
    conf.stepPin = stepP;
    conf.dirPin = dirP;
    conf.enPin = enP;
    conf.timer_group = group;
    conf.timer_idx = index;
    conf.miStep = microstepping;
    ctrl.status = 0;
    init();
}

void DendoStepper::init()
{
    uint64_t mask = (1ULL << conf.stepPin) | (1ULL << conf.dirPin) | (1ULL << conf.enPin); // put output gpio pins in bitmask
    gpio_config_t gpio_conf = {
        // config gpios
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    // set the gpios as per gpio_conf
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));

    timer_config_t timer_conf = {
        .alarm_en = TIMER_ALARM_EN,         // we need alarm
        .counter_en = TIMER_PAUSE,          // dont start now lol
        .intr_type = TIMER_INTR_LEVEL,      // interrupt
        .counter_dir = TIMER_COUNT_UP,      // count up duh
        .auto_reload = TIMER_AUTORELOAD_EN, // reload pls
        .divider = 80000000ULL / TIMER_F,   // ns resolution
    };

    // calculate stepsPerRot
    ctrl.stepsPerRot = (360.0 / conf.stepAngle) * conf.miStep;

    STEP_LOGI("DendoStepper", "Steps per one rotation:%d", ctrl.stepsPerRot);

    if (conf.timer_group != TIMER_GROUP_MAX && conf.timer_idx != TIMER_MAX)
    {
        // timer was configured explixitly in config structure, we dont need to do it here
        goto timer_avail;
    }

    // try to find free timer
    timer_config_t temp;
    for (int i = 0; i < 1; i++)
    {
        for (int j = 0; j < 1; j++)
        {
            timer_get_config((timer_group_t)i, (timer_idx_t)j, &temp);
            if (temp.alarm_en == TIMER_ALARM_DIS)
            {
                // if the alarm is disabled, chances are no other dendostepper instance is using it
                conf.timer_group = (timer_group_t)i;
                conf.timer_idx = (timer_idx_t)j;
                goto timer_avail;
            }
        }
    }

    // if we got here it means that there isnt any free timer available
    STEP_LOGE("DendoStepper", "No free timer available, this stepper wont work");
    return;

timer_avail:
    ESP_ERROR_CHECK(timer_init(conf.timer_group, conf.timer_idx, &timer_conf));                   // init the timer
    ESP_ERROR_CHECK(timer_set_counter_value(conf.timer_group, conf.timer_idx, 0));                // set it to 0
    ESP_ERROR_CHECK(timer_isr_callback_add(conf.timer_group, conf.timer_idx, xISRwrap, this, 0)); // add callback fn to run when alarm is triggrd
}

esp_err_t DendoStepper::runPos(int32_t relative)
{
    //TODO only enable when actually moving
    if (ctrl.status == DISABLED) // if motor is disabled, enable it
        enableMotor();

    if (!relative) // why would u call it with 0 wtf
        return ESP_ERR_NOT_SUPPORTED;

    if (ctrl.status > IDLE) { //currently moving
        bool newDir = (relative < 0); // CCW if <0, else set CW
        if (ctrl.dir == newDir){ //current direction is the same
            ctrl.statusPrev = ctrl.status; //update previous status
            ctrl.status = ctrl.status==COAST ? COAST : ACC; //stay at coast otherwise switch to ACC
            calc(abs(relative + ctrl.stepsRemaining)); //calculate new velolcity profile for new+remaining steps
        } else { //direction has changed
                 //direction change not supported TODO wait for decel finish / queue?
            STEP_LOGW("DendoStepper", "DIRECTION HOT-CHANGE NOT SUPPORTED - Finising previous move, this command will be ignored");
            return ESP_ERR_NOT_SUPPORTED;
        }
    }
    else { //current state is IDLE
        ctrl.statusPrev = ctrl.status; //update previous status
        ctrl.status = ACC;
        setDir(relative < 0); // set CCW if <0, else set CW
        calc(abs(relative));                                                                         // calculate velocity profile
    }

    currentPos += relative; //(target position / not actual)
    ESP_ERROR_CHECK(timer_set_alarm_value(conf.timer_group, conf.timer_idx, ctrl.stepInterval)); // set HW timer alarm to stepinterval
                                                                                                 //TODO timer has to be stopped before update if already running?
    ESP_ERROR_CHECK(timer_start(conf.timer_group, conf.timer_idx));                              // start the timer

    return ESP_OK;
}

esp_err_t DendoStepper::runPosMm(int32_t relative)
{
    if (ctrl.stepsPerMm == 0)
    {
        STEP_LOGE("DendoStepper", "Steps per millimeter not set, cannot move!");
    }
    return runPos(relative * ctrl.stepsPerMm);
}

esp_err_t DendoStepper::runAbs(uint32_t position)
{
    if (getState() > IDLE) // we are already moving, so stop it
        stop();
    while (getState() > IDLE)
    {
        // waiting for idle, watchdog should take care of inf loop if it occurs
        vTaskDelay(1);
    } // shouldnt take long tho

    if (position == currentPos) // we cant go anywhere
        return 0;

    int32_t relativeSteps = 0;
    relativeSteps = (int32_t)position - currentPos;

    ESP_LOGI("DendoStepper", "Cur: %llu move %d", currentPos, relativeSteps);
    return runPos(relativeSteps); // run to new position
}

esp_err_t DendoStepper::runAbsMm(uint32_t position)
{
    if (ctrl.stepsPerMm == 0)
    {
        STEP_LOGE("DendoStepper", "Steps per millimeter not set, cannot move!");
    }
    return runAbs(position * ctrl.stepsPerMm);
}

void DendoStepper::setSpeed(uint32_t speed, uint16_t accT, uint16_t decT)
{
    ctrl.speed = speed;
    ctrl.acc = ctrl.speed / (accT / 4000.0);
    ctrl.dec = ctrl.speed / (decT / 4000.0);
    STEP_LOGI("DendoStepper", "Speed set: %d steps/s t+=%d s t-=%d s", speed, accT, decT);
}

void DendoStepper::setSpeedMm(uint32_t speed, uint16_t accT, uint16_t decT)
{
    if (ctrl.stepsPerMm == 0)
    {
        STEP_LOGE("DendoStepper", "Steps per millimeter not set, cannot set the speed!");
    }
    ctrl.speed = speed * ctrl.stepsPerMm;

    ctrl.acc = ctrl.speed / (accT / 4000.0);
    ctrl.dec = ctrl.speed / (decT / 4000.0);
    STEP_LOGI("DendoStepper", "Speed set: v=%d mm/s t+=%d s t-=%d s", speed, accT, decT);
}

//CUSTOM - change speed while running 
//FIXME: this approach does not work, since calc function would have to be run after change, this will mess up target steps...
//void DendoStepper::changeSpeed(uint32_t speed)
//{
//    //TODO reduce duplicate code (e.g. call setSpeed function)
//    //change speed
//    ctrl.speed = speed;
//    //change status to ACC/DEC
//    STEP_LOGI("DendoStepper", "Speed changed: from v=%.2f rad/s to v=%.2f rad/s", ctrl.speed, speed);
//    if (speed > ctrl.speed) ctrl.status = ACC;
//    if (speed < ctrl.speed) ctrl.status = DEC;
//}
//
//void DendoStepper::changeSpeedMm(uint32_t speed)
//{
//    //TODO reduce duplicate code (e.g. call setSpeedMm function)
//    if (ctrl.stepsPerMm == 0)
//    {
//        STEP_LOGE("DendoStepper", "Steps per millimeter not set, cannot set the speed!");
//    }
//    //calc new speed
//    float speedNew = speed * ctrl.stepsPerMm;
//    //change status to ACC/DEC
//    if (speedNew > ctl.speed) ctrl.status = ACC;
//    if (speedNew < ctl.speed) ctrl.status = DEC;
//    //update speed, log output
//    ctrl.speed = speedNew;
//    STEP_LOGI("DendoStepper", "Speed changed: from v=%.2f rad/s to v=%.2f rad/s", ctrl.speed, speedNew);
//}


void DendoStepper::setStepsPerMm(uint16_t steps)
{
    ctrl.stepsPerMm = steps;
}

uint16_t DendoStepper::getStepsPerMm()
{
    return ctrl.stepsPerMm;
}

uint8_t DendoStepper::getState()
{
    return ctrl.status;
}

uint64_t DendoStepper::getPosition()
{
    return currentPos;
}

uint64_t DendoStepper::getPositionMm()
{
    return getPosition() / ctrl.stepsPerMm;
}

void DendoStepper::resetAbsolute()
{
    currentPos = 0;
}

void DendoStepper::runInf(bool direction)
{
    if (ctrl.status > IDLE)
    {
        STEP_LOGE("DendoStepper", "Motor is moving, this command will be ignored");
        return;
    }
    if (ctrl.status == DISABLED)
    {
        enableMotor();
    }
    ctrl.runInfinite = true;
    setDir(direction);
    calc(UINT32_MAX);
    ESP_ERROR_CHECK(timer_set_alarm_value(conf.timer_group, conf.timer_idx, ctrl.stepInterval)); // set HW timer alarm to stepinterval
    ESP_ERROR_CHECK(timer_start(conf.timer_group, conf.timer_idx));                              // start the timer
}

uint16_t DendoStepper::getSpeed()
{
    return ctrl.speed;
}

float DendoStepper::getAcc()
{
    return ctrl.acc;
}

void DendoStepper::stop()
{
    if (ctrl.status <= IDLE)
    {
        return;
    }
    ctrl.runInfinite = false;
    timer_pause(conf.timer_group, conf.timer_idx); // stop the timer
    ctrl.statusPrev = ctrl.status; //update previous status
    ctrl.status = IDLE;
    ctrl.stepCnt = 0;
    gpio_set_level((gpio_num_t)conf.stepPin, 0);
}

// PRIVATE definitions

void DendoStepper::setEn(bool state)
{
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)conf.enPin, state));
}

void DendoStepper::setDir(bool state)
{
    ctrl.dir = state;
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)conf.dirPin, state));
}

/* Timer callback, used for generating pulses and calculating speed profile in real time */
bool DendoStepper::xISR()
{
    GPIO.out_w1ts = (1ULL << conf.stepPin);
    // add and substract one step

    ctrl.stepCnt++;

    //CUSTOM: track actual precice current position
    if (ctrl.dir) {
        ctrl.posActual ++;
    } else {
        ctrl.posActual --;
    }
    //CUSTOM: track remaining steps for eventually resuming
    ctrl.stepsRemaining = ctrl.stepCnt - ctrl.stepCnt; 

    // we are done
    if (ctrl.stepsToGo == ctrl.stepCnt && !ctrl.runInfinite)
    {
        timer_pause(conf.timer_group, conf.timer_idx); // stop the timer
        ctrl.statusPrev = ctrl.status; //update previous status
        ctrl.status = IDLE;
        ctrl.stepCnt = 0;
        return 0;
    }

    if (ctrl.stepCnt > 0 && ctrl.stepCnt < ctrl.accEnd)
    { // we are accelerating
        ctrl.currentSpeed += ctrl.accInc;
        ctrl.statusPrev = ctrl.status; //update previous status
        ctrl.status = ACC; // we are accelerating, note that*/
    }
    else if (ctrl.stepCnt > ctrl.coastEnd && !ctrl.runInfinite)
    { // we must be deccelerating then
        ctrl.currentSpeed -= ctrl.decInc;
        ctrl.statusPrev = ctrl.status; //update previous status
        ctrl.status = DEC; // we are deccelerating
    }
    else
    {
        ctrl.currentSpeed = ctrl.targetSpeed;
        ctrl.statusPrev = ctrl.status; //update previous status
        ctrl.status = COAST; // we are coasting
    }

    ctrl.stepInterval = TIMER_F / ctrl.currentSpeed;
    // set alarm to calculated interval and disable pin
    GPIO.out_w1tc = (1ULL << conf.stepPin);
    timer_set_alarm_value(conf.timer_group, conf.timer_idx, ctrl.stepInterval);
    ctrl.stepCnt++;
    return 1;
}

void DendoStepper::calc(uint32_t targetSteps)
{
    //steps from ctrl.speed -> 0:
    ctrl.decSteps = 0.5 * ctrl.dec * (ctrl.speed / ctrl.dec) * (ctrl.speed / ctrl.dec);
    //steps from 0 -> ctrl.speed:
    //ctrl.accSteps = 0.5 * ctrl.acc * (ctrl.speed / ctrl.acc) * (ctrl.speed / ctrl.acc);
    //steps from ctrl.currentSpeed -> ctrl.speed:
    ctrl.accSteps = 0.5 * ctrl.acc * (ctrl.speed / ctrl.acc) * (ctrl.speed / ctrl.acc)  * (ctrl.speed - ctrl.currentSpeed) / ctrl.speed;

    if (targetSteps < (ctrl.decSteps + ctrl.accSteps))
    {
        ESP_LOGI("Dendostepper", "Computing new speed");

        ctrl.speed = sqrt(2 * targetSteps * ((ctrl.dec * ctrl.acc) / (ctrl.dec + ctrl.acc)));
        ctrl.accSteps = 0.5 * ctrl.acc * (ctrl.speed / ctrl.acc) * (ctrl.speed / ctrl.acc);
        ctrl.decSteps = 0.5 * ctrl.dec * (ctrl.speed / ctrl.dec) * (ctrl.speed / ctrl.dec);
    }

    ctrl.accEnd = ctrl.accSteps;
    ctrl.coastEnd = targetSteps - ctrl.decSteps;
    ctrl.targetSpeed = ctrl.speed;

    ctrl.accInc = ctrl.targetSpeed / (double)ctrl.accSteps;
    ctrl.decInc = ctrl.targetSpeed / (double)ctrl.decSteps;

    ctrl.currentSpeed = ctrl.accInc;

    ctrl.stepInterval = TIMER_F / ctrl.currentSpeed;
    ctrl.stepsToGo = targetSteps;

    STEP_LOGI("calc", "acc end:%u coastend:%u stepstogo:%u speed:%f acc:%f int: %u", ctrl.accEnd, ctrl.coastEnd, ctrl.stepsToGo, ctrl.speed, ctrl.acc, ctrl.stepInterval);
}
