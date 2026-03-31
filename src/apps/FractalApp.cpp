#include "FractalApp.h"
#include "../input/KeyboardManager.h"
#include <cstring>

FractalApp::FractalApp() 
    : _screen(nullptr),
      _canvasArea(nullptr),
      _fractalImage(nullptr),
      _centerX(-0.5f),    // standard offset for full mandelbrot
      _centerY(0.0f),
      _zoom(1.0f),        // standard initial zoom
      _maxIter(64)        // low iter for fast initial render
{
    // Make sure descriptor is clean
    memset(&_imgDsc, 0, sizeof(lv_image_dsc_t));
    memset(&_currentJob, 0, sizeof(RenderJob));
}

FractalApp::~FractalApp() {
    end();
}

void FractalApp::begin() {
    if (_screen) return; // Already begun

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x000000), 0);

    createUI();
    initializeBuffer();

    _renderRequested = false;
    _renderComplete  = false;
    _abortRender     = false;
    _renderAborted   = true;
    _taskShouldExit  = false;
    _taskExited      = false;

#ifdef ARDUINO
    xTaskCreatePinnedToCore(
        renderTaskWrapper,     // Task function
        "fractalTask",         // Name
        8192,                  // Stack size
        this,                  // Parameters
        1,                     // Priority
        &_renderTaskHandle,    // Task handle
        1                      // Core 1
    );
#endif

    // Timer to poll _renderComplete at ~30 FPS (33 ms)
    _updateTimer = lv_timer_create(checkStatusTimer, 33, this);
}

void FractalApp::end() {
    if (_updateTimer) {
        lv_timer_delete(_updateTimer);
        _updateTimer = nullptr;
    }

#ifdef ARDUINO
    if (_renderTaskHandle) {
        // Signal task to exit and abort any running process
        _abortRender = true;
        _taskShouldExit = true;

        // Wake task if it's currently waiting for a notification
        xTaskNotifyGive(_renderTaskHandle);

        // Wait cleanly for task to terminate itself
        while (!_taskExited.load(std::memory_order_acquire)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        _renderTaskHandle = nullptr; // Task deleted itself or exited successfully
    }
#endif

    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
    }
    _statusBar.resetPointers();
    _buffer.reset(); // Free PSRAM
}

void FractalApp::load() {
    if (!_screen) {
        begin();
    }
    
    // Load screen into active display
    lv_screen_load(_screen);
    _statusBar.update();

    // Do an initial render
    requestRender(true);
}

void FractalApp::handleKey(const KeyEvent& ev) {
    // Only care about pressing down or repeat
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) {
        return;
    }

    const int shiftPixels = 15; // amount to shift visually per keystroke

    float windowW = 3.0f / _zoom;
    float aspect = (float)SCREEN_W / (float)CANVAS_H;
    float windowH = windowW / aspect;

    float cDeltaX = (windowW / SCREEN_W) * shiftPixels;
    float cDeltaY = (windowH / CANVAS_H) * shiftPixels;

    switch (ev.code) {
        case KeyCode::UP:
            _centerY += cDeltaY;
            shiftBuffer(0, shiftPixels);
            break;
        case KeyCode::DOWN:
            _centerY -= cDeltaY;
            shiftBuffer(0, -shiftPixels);
            break;
        case KeyCode::LEFT:
            _centerX -= cDeltaX;
            shiftBuffer(shiftPixels, 0);
            break;
        case KeyCode::RIGHT:
            _centerX += cDeltaX;
            shiftBuffer(-shiftPixels, 0);
            break;
        case KeyCode::ADD:
            _zoom *= 1.5f;
            scaleBuffer(1.5f);
            break;
        case KeyCode::SUB:
            _zoom /= 1.5f;
            scaleBuffer(1.0f / 1.5f);
            break;
        case KeyCode::MUL:
            _maxIter += 32;
            requestRender(true);
            break;
        case KeyCode::DIV:
            if (_maxIter > 32) _maxIter -= 32;
            requestRender(true);
            break;
        default:
            break;
    }
}

void FractalApp::createUI() {
    // Status Bar
    _statusBar.create(_screen);
    _statusBar.setTitle("Fractals");

    // Canvas container
    _canvasArea = lv_obj_create(_screen);
    lv_obj_set_size(_canvasArea, SCREEN_W, CANVAS_H);
    lv_obj_align(_canvasArea, LV_ALIGN_TOP_LEFT, 0, BAR_H);
    
    lv_obj_set_scrollbar_mode(_canvasArea, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(_canvasArea, LV_OBJ_FLAG_SCROLLABLE);

    // Padding & border removal
    lv_obj_set_style_pad_all(_canvasArea, 0, 0);
    lv_obj_set_style_border_width(_canvasArea, 0, 0);
    lv_obj_set_style_bg_color(_canvasArea, lv_color_hex(0x000000), 0);

    // Image Object
    _fractalImage = lv_image_create(_canvasArea);
    lv_obj_align(_fractalImage, LV_ALIGN_TOP_LEFT, 0, 0);

    // Loading Label ("Rendering...")
    _loadingLabel = lv_label_create(_screen);
    lv_label_set_text(_loadingLabel, "Rendering...");
    lv_obj_set_style_text_color(_loadingLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(_loadingLabel, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_add_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN); // Hidden by default
}

void FractalApp::initializeBuffer() {
    // Allocate 320 x 215 buffer in PSRAM
    if (!_buffer.allocate(SCREEN_W * CANVAS_H)) {
        // Fallback or error
        return;
    }

    _imgDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    _imgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    _imgDsc.header.flags = 0;
    _imgDsc.header.w = SCREEN_W;
    _imgDsc.header.h = CANVAS_H;
    _imgDsc.header.stride = SCREEN_W * 2; 
    _imgDsc.data_size = SCREEN_W * CANVAS_H * 2;
    _imgDsc.data = (const uint8_t*)_buffer.data();
}

void FractalApp::requestRender(bool full) {
    if (!_buffer.data()) return;

    _abortRender = true;
#ifdef ARDUINO
    while (!_renderAborted.load(std::memory_order_acquire)) vTaskDelay(1);
#endif
    
    _currentJob.centerX = _centerX;
    _currentJob.centerY = _centerY;
    _currentJob.zoom    = _zoom;
    _currentJob.maxIter = _maxIter;
    _currentJob.fullRender = full;
    _currentJob.numRects = 0;

    lv_obj_remove_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);

    _abortRender = false;
    _renderAborted = false;

#ifdef ARDUINO
    _renderRequested = true;
    if (_renderTaskHandle) xTaskNotifyGive(_renderTaskHandle);
#else
    // PC Emulator synchronous call
    FractalEngine::renderMandelbrot(
        _buffer.data(), SCREEN_W, CANVAS_H,
        _centerX, _centerY, _zoom, _maxIter, 
        0, 0, SCREEN_W, CANVAS_H, &_abortRender, true
    );
    _renderAborted = true;
    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);
    lv_obj_add_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);
#endif
}

void FractalApp::requestRenderRects(const RenderRect& r1, const RenderRect& r2, int count) {
    _abortRender = true;
#ifdef ARDUINO
    while (!_renderAborted.load(std::memory_order_acquire)) vTaskDelay(1);
#endif
    
    _currentJob.centerX    = _centerX;
    _currentJob.centerY    = _centerY;
    _currentJob.zoom       = _zoom;
    _currentJob.maxIter    = _maxIter;
    _currentJob.fullRender = false;
    _currentJob.numRects   = count;
    
    if (count > 0) _currentJob.rects[0] = r1;
    if (count > 1) _currentJob.rects[1] = r2;

    lv_obj_remove_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);

    _abortRender = false;
    _renderAborted = false;

#ifdef ARDUINO
    _renderRequested = true;
    if (_renderTaskHandle) xTaskNotifyGive(_renderTaskHandle);
#endif
}

void FractalApp::scaleBuffer(float scaleFactor) {
    uint16_t* p = _buffer.data();
    if (!p) return;

    _abortRender = true;
#ifdef ARDUINO
    while (!_renderAborted.load(std::memory_order_acquire)) vTaskDelay(1);
#endif
    
    // Allocate scratch PSRAM buffer
    utils::PSRAMBuffer<uint16_t> scratch;
    if (scratch.allocate(SCREEN_W * CANVAS_H)) {
        uint16_t* t = scratch.data();
        int cx = SCREEN_W / 2;
        int cy = CANVAS_H / 2;
        
        for (int y = 0; y < CANVAS_H; y++) {
            float srcY = cy + (y - cy) / scaleFactor;
            int iy = (int)srcY;
            if (iy < 0) iy = 0; else if (iy >= CANVAS_H) iy = CANVAS_H - 1;
            for (int x = 0; x < SCREEN_W; x++) {
                float srcX = cx + (x - cx) / scaleFactor;
                int ix = (int)srcX;
                if (ix < 0) ix = 0; else if (ix >= SCREEN_W) ix = SCREEN_W - 1;
                t[y * SCREEN_W + x] = p[iy * SCREEN_W + ix];
            }
        }
        memcpy(p, t, SCREEN_W * CANVAS_H * sizeof(uint16_t));
    }
    
    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);
    requestRender(true);
}

void FractalApp::shiftBuffer(int dx, int dy) {
    uint16_t* p = _buffer.data();
    if (!p) return;

    _abortRender = true;
#ifdef ARDUINO
    while (!_renderAborted.load(std::memory_order_acquire)) vTaskDelay(1);
#endif

    if (dy != 0) {
        int rowBytes = SCREEN_W * sizeof(uint16_t);
        if (dy > 0) memmove(p + dy * SCREEN_W, p, (CANVAS_H - dy) * rowBytes);
        else memmove(p, p - dy * SCREEN_W, (CANVAS_H + dy) * rowBytes);
    }
    
    if (dx != 0) {
        for (int y = 0; y < CANVAS_H; y++) {
            if (dx > 0) memmove(p + y * SCREEN_W + dx, p + y * SCREEN_W, (SCREEN_W - dx) * sizeof(uint16_t));
            else memmove(p + y * SCREEN_W, p + y * SCREEN_W - dx, (SCREEN_W + dx) * sizeof(uint16_t));
        }
    }

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);

    RenderRect rects[2];
    int count = 0;

    if (dy > 0) {
        rects[count++] = {0, 0, SCREEN_W, dy};  // New top edge
    } else if (dy < 0) {
        rects[count++] = {0, CANVAS_H + dy, SCREEN_W, CANVAS_H}; // New bottom edge
    }

    if (dx > 0) {
        int sy = (dy > 0) ? dy : 0;
        int ey = (dy < 0) ? CANVAS_H + dy : CANVAS_H;
        rects[count++] = {0, sy, dx, ey};
    } else if (dx < 0) {
        int sy = (dy > 0) ? dy : 0;
        int ey = (dy < 0) ? CANVAS_H + dy : CANVAS_H;
        rects[count++] = {SCREEN_W + dx, sy, SCREEN_W, ey};
    }

    requestRenderRects(rects[0], rects[1], count);
}

void FractalApp::renderTaskWrapper(void* param) {
#ifdef ARDUINO
    FractalApp* app = static_cast<FractalApp*>(param);
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Immediate exit check
        if (app->_taskShouldExit.load(std::memory_order_relaxed)) {
            break;
        }

        if (app->_renderRequested && app->_buffer.data()) {
            app->_renderRequested = false;

            RenderJob job = app->_currentJob;

            if (job.fullRender || job.numRects == 0) {
                FractalEngine::renderMandelbrot(
                    app->_buffer.data(),
                    FractalApp::SCREEN_W, FractalApp::CANVAS_H,
                    job.centerX, job.centerY, job.zoom, job.maxIter,
                    0, 0, FractalApp::SCREEN_W, FractalApp::CANVAS_H,
                    &app->_abortRender, true
                );
            } else {
                for (int i = 0; i < job.numRects; i++) {
                    FractalEngine::renderMandelbrot(
                        app->_buffer.data(),
                        FractalApp::SCREEN_W, FractalApp::CANVAS_H,
                        job.centerX, job.centerY, job.zoom, job.maxIter,
                        job.rects[i].startX, job.rects[i].startY, 
                        job.rects[i].endX, job.rects[i].endY,
                        &app->_abortRender, true
                    );
                }
            }

            // Acknowledge aborted state securely
            if (app->_abortRender.load(std::memory_order_relaxed)) {
                app->_renderAborted.store(true, std::memory_order_release);
            } else {
                app->_renderAborted.store(true, std::memory_order_release);
                app->_renderComplete = true; // Tell LVGL we actually finished rendering organically!
            }
        }
    }
    
    // Task breakdown
    app->_taskExited.store(true, std::memory_order_release);
    vTaskDelete(NULL); // Deletes itself!
#endif
}

void FractalApp::checkStatusTimer(lv_timer_t* timer) {
    FractalApp* app = static_cast<FractalApp*>(lv_timer_get_user_data(timer));
    if (app && app->_renderComplete) {
        app->_renderComplete = false;
        lv_image_set_src(app->_fractalImage, &app->_imgDsc);
        lv_obj_invalidate(app->_fractalImage);
        lv_obj_add_flag(app->_loadingLabel, LV_OBJ_FLAG_HIDDEN);
    }
}
