#pragma once

#include <lvgl.h>
#include "../utils/MemoryUtils.h"
#include "../input/KeyCodes.h"
#include "../ui/StatusBar.h"
#include "../math/FractalEngine.h"

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include "../hal/ArduinoCompat.h"
#endif

#include <atomic>

struct RenderRect {
    int startX, startY;
    int endX, endY;
};

struct RenderJob {
    float centerX;
    float centerY;
    float zoom;
    int maxIter;
    bool fullRender;
    RenderRect rects[2]; // At most 2 rectangles for L-shaped new regions
    int numRects;
};

class FractalApp {
public:
    FractalApp();
    ~FractalApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;
    static constexpr int BAR_H = 25; // Status bar height
    static constexpr int CANVAS_H = SCREEN_H - BAR_H;

    lv_obj_t* _screen;
    ui::StatusBar _statusBar;

    // UI elements
    lv_obj_t* _canvasArea;
    lv_obj_t* _fractalImage;
    lv_obj_t* _loadingLabel;

    // Image buffer (PSRAM-backed for ESP32-S3)
    utils::PSRAMBuffer<uint16_t> _buffer;
    lv_image_dsc_t _imgDsc;

    // Viewport parameters
    float _centerX;
    float _centerY;
    float _zoom;
    int   _maxIter;

    // Dual-core FreeRTOS rendering variables
    TaskHandle_t _renderTaskHandle = nullptr;
    lv_timer_t*  _updateTimer      = nullptr;
    volatile bool _renderRequested = false;
    volatile bool _renderComplete  = false;
    
    std::atomic<bool> _abortRender{false};
    std::atomic<bool> _renderAborted{true};
    std::atomic<bool> _taskShouldExit{false};
    std::atomic<bool> _taskExited{false};

    RenderJob             _currentJob;

    void createUI();
    void initializeBuffer();
    // Replaced renderFractal() with these finer-grain methods
    void requestRender(bool full);
    void requestRenderRects(const RenderRect& r1, const RenderRect& r2, int count);
    void shiftBuffer(int dx, int dy);
    void scaleBuffer(float scaleFactor);

    static void renderTaskWrapper(void* param);
    static void checkStatusTimer(lv_timer_t* timer);
};
