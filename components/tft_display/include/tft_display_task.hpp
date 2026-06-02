#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class TftDisplayTask {
public:
    TftDisplayTask();
    ~TftDisplayTask();

    bool init();
    static void TaskWrapper(void* pvParameters);

private:
    void run();
    bool m_initialized;
    StaticTask_t m_taskBuffer;
    StackType_t* m_taskStack;
};
