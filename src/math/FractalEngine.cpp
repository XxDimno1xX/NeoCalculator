#include "FractalEngine.h"

// ── Simple RGB565 macro ──
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void FractalEngine::renderMandelbrot(uint16_t* buffer, int width, int height, 
                                     float centerX, float centerY, float zoom, 
                                     int maxIter, int startX, int startY, int endX, int endY,
                                     std::atomic<bool>* abortRender,
                                     bool invertY) {
    // 1.5f and 1.0f are base aspect ratio bounds (roughly 3:2)
    // Scale by 1.0/zoom to get world window size
    float aspect = (float)width / (float)height;
    
    float windowW = 3.0f / zoom;
    float windowH = windowW / aspect; 

    float xMin = centerX - windowW / 2.0f;
    float yMin = centerY - windowH / 2.0f;

    float dx = windowW / (float)width;
    float dy = windowH / (float)height;

    // Constrain rect to screen bounds
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;
    if (endX > width) endX = width;
    if (endY > height) endY = height;

    for (int py = startY; py < endY; py++) {
        // Check for strict abort at each row
        if (abortRender && abortRender->load(std::memory_order_relaxed)) {
            return;
        }

        // Handle inverted graphics Y axis if necessary
        int screenY = invertY ? (height - 1 - py) : py;
        float cy = yMin + py * dy;

        for (int px = startX; px < endX; px++) {
            float cx = xMin + px * dx;
            float zx = 0;
            float zy = 0;
            int iter = 0;
            
            // Standard z_{n+1} = z_n^2 + c
            while ((zx * zx + zy * zy) <= 4.0f && iter < maxIter) {
                float tempX = zx * zx - zy * zy + cx;
                zy = 2.0f * zx * zy + cy;
                zx = tempX;
                iter++;
            }

            buffer[screenY * width + px] = mapColor(iter, maxIter);
        }
    }
}

uint16_t FractalEngine::mapColor(int iter, int maxIter) {
    if (iter == maxIter) return 0x0000; // Black for interior

    // Simple smooth coloring: continuous base offset combined with some modulo
    // We create a basic multi-gradient palette
    float t = (float)iter / (float)maxIter;
    
    // Create a psychedelic cyan/blue/white/black gradient like typical Mandelbrot sets
    uint8_t r = (uint8_t)(9.0f * (1.0f - t) * t * t * t * 255.0f);
    uint8_t g = (uint8_t)(15.0f * (1.0f - t) * (1.0f - t) * t * t * 255.0f);
    uint8_t b = (uint8_t)(8.5f * (1.0f - t) * (1.0f - t) * (1.0f - t) * t * 255.0f);

    return rgb565(r, g, b);
}
