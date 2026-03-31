#pragma once

#include <cstdint>
#include <atomic>

class FractalEngine {
public:
    /**
     * Renders a Mandelbrot fractal to a 1D pixel buffer.
     * Use standard 32-bit floating point math.
     * @param buffer 1D RGB565 pixel buffer (height * width size)
     * @param width  Buffer width in pixels
     * @param height Buffer height in pixels
     * @param centerX Center X on complex plane
     * @param centerY Center Y on complex plane
     * @param zoom Zoom level (larger = more zoomed in)
     * @param maxIter Maximum iterations for escape time
     * @param startX Start X index for partial render
     * @param startY Start Y index for partial render
     * @param endX End X index (exclusive)
     * @param endY End Y index (exclusive)
     * @param abortRender Pointer to an atomic boolean flag. If true, the renderer immediately exits.
     * @param invertY Whether to invert the Y axis (default true for standard graphics coordinates)
     */
    static void renderMandelbrot(uint16_t* buffer, int width, int height, 
                                 float centerX, float centerY, float zoom, 
                                 int maxIter, int startX, int startY, int endX, int endY,
                                 std::atomic<bool>* abortRender = nullptr,
                                 bool invertY = true);
    
    /**
     * Maps an iteration count to an RGB565 color.
     */
    static uint16_t mapColor(int iter, int maxIter);
};
