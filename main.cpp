#define _USE_MATH_DEFINES
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdlib.h>

// Core Logic của bạn
#include "AudioSignal.h"
#include "AudioComponents.h"
#include "AudioFile.h"

// Thư viện Đồ họa
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION 
#include "raygui.h"

using namespace std;
void DrawCheckmark(Rectangle bounds) {
    // 1. Tô đè một hình chữ nhật tàng hình (màu giống nền Panel) để che ô vuông xám đi
    DrawRectangle(bounds.x + 4, bounds.y + 4, bounds.width - 8, bounds.height - 8, GetColor(0x383838ff));
    
    // 2. Tính toán tọa độ 3 điểm để nối thành hình chữ V (dấu tích)
    Vector2 p1 = { bounds.x + 6, bounds.y + bounds.height / 2 + 2 };
    Vector2 p2 = { bounds.x + bounds.width / 2 - 2, bounds.y + bounds.height - 4 };
    Vector2 p3 = { bounds.x + bounds.width - 4, bounds.y + 6 };
    
    // 3. Vẽ 2 đoạn thẳng nối nhau với độ dày 3.5 pixel, màu Xanh Neon
    DrawLineEx(p1, p2, 3.5f, GREEN);
    DrawLineEx(p2, p3, 3.5f, GREEN);
}
// Hàm cầu nối phát âm thanh
void PlaySignalWithRaylib(const AudioSignal& sig, Sound& currentSound) {
    if (currentSound.frameCount > 0) {
        StopSound(currentSound);
        UnloadSound(currentSound);
    }
    const auto& samples = sig.getSamples();
    if (samples.empty()) return;

    float* floatData = (float*)MemAlloc(samples.size() * sizeof(float));
    for (size_t i = 0; i < samples.size(); i++) floatData[i] = static_cast<float>(samples[i]);

    Wave wave = { 0 };
    wave.frameCount = samples.size();
    wave.sampleRate = sig.getSampleRate();
    wave.sampleSize = 32;
    wave.channels = sig.getNumChannels();
    wave.data = floatData;

    currentSound = LoadSoundFromWave(wave);
    PlaySound(currentSound);
    UnloadWave(wave);
}

int main() {
    // 1. KHỞI TẠO CỬA SỔ & HỆ THỐNG
    const int screenWidth = 1400;
    const int screenHeight = 900;
    InitWindow(screenWidth, screenHeight, "DIGITAL AUDIO WORKSTATION (DAW) - BIG GUI");
    InitAudioDevice();
    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);

    AudioProcessor  processor;
    AudioEffect     effect(0.5, 100.0);
    AudioFile       audioFile("output.wav");
    Sound           activeSound = { 0 };

    // --- CÁC BIẾN TRẠNG THÁI GIAO DIỆN ---
    // 1 & 2. Generator
    int waveTypeActive = 0; // 0:Sine, 1:Square, 2:Tri, 3:Noise, 4:DTMF
    float frequency = 440.0f;
    
    // 3. Modifiers (Khuyếch đại, Chuẩn hóa)
    float amplifyFactor = 1.0f;
    bool doNormalize = false;

    // 4. Digital Filters (Bộ lọc số)
    int filterType = 0; // 0:None, 1:LowPass, 2:HighPass
    float cutoffFreq = 500.0f;

    // 5. Audio Effects (Hiệu ứng)
    bool fxEcho = false, fxReverb = false, fxDistort = false, fxTremolo = false;

    // 8. Mixing & 9. Fade
    bool doMixDrum = false;
    bool doFadeInOut = false;

    // Phân tích phổ (Spectrum)
    vector<double> spectrumBins(15, 0.0);

    // Khởi tạo tín hiệu ban đầu
    AudioSignal currentSignal = AudioGenerator::generateSine(frequency, 2.0);
    bool forceRecalculate = true; // Cờ yêu cầu tính toán lại chuỗi âm thanh

    while (!WindowShouldClose()) {
        bool signalChanged = false;

        BeginDrawing();
        ClearBackground(GetColor(0x2b2b2bff)); // Nền Dark Mode chuyên nghiệp

        // =========================================================
        // PANEL 1: ĐIỀU KHIỂN CHÍNH (BÊN TRÁI)
        // =========================================================
        DrawRectangle(15, 15, 350, 420, GetColor(0x383838ff));
        DrawText("SIGNAL GENERATOR", 30, 30, 16, LIGHTGRAY);
        
        // Chọn sóng & Tần số
        float oldFreq = frequency;
        GuiSlider((Rectangle){ 100, 60, 240, 20 }, "Freq (Hz)", TextFormat("%.0f", frequency), &frequency, 50.0f, 2000.0f);
        if (abs(frequency - oldFreq) > 1.0f) signalChanged = true;

        // Bộ lọc số (Digital Filter)
        DrawText("DIGITAL FILTERS", 30, 130, 16, LIGHTGRAY);
        float oldCutoff = cutoffFreq;
        GuiSlider((Rectangle){ 100, 160, 240, 20 }, "Cutoff", TextFormat("%.0f Hz", cutoffFreq), &cutoffFreq, 100.0f, 3000.0f);
        if (abs(cutoffFreq - oldCutoff) > 10.0f) signalChanged = true;

        int oldFilter = filterType;
        GuiComboBox((Rectangle){ 100, 190, 240, 20 }, "None;Low-Pass Filter;High-Pass Filter;Band-Pass Filter", &filterType);
        if (filterType != oldFilter) signalChanged = true;

        // Các Modify khác
        DrawText("MODIFIERS & 8. MIXING", 30, 240, 16, LIGHTGRAY);
        float oldAmp = amplifyFactor;
        GuiSlider((Rectangle){ 100, 270, 240, 20 }, "Amplify", TextFormat("x%.1f", amplifyFactor), &amplifyFactor, 0.1f, 3.0f);
        if (abs(amplifyFactor - oldAmp) > 0.1f) signalChanged = true;

        bool oldNorm = doNormalize; GuiCheckBox((Rectangle){ 30, 310, 20, 20 }, "Normalize (Chuan hoa)", &doNormalize);
        bool oldFade = doFadeInOut; GuiCheckBox((Rectangle){ 30, 340, 20, 20 }, "Fade In / Fade Out (2000 samples)", &doFadeInOut);
        bool oldMix  = doMixDrum;   GuiCheckBox((Rectangle){ 30, 370, 20, 20 }, "Mix with Drum Noise (Tron tin hieu)", &doMixDrum);
        if (doNormalize != oldNorm || doFadeInOut != oldFade || doMixDrum != oldMix) signalChanged = true;

        // =========================================================
        // PANEL 2: HIỆU ỨNG & CÔNG CỤ (GIỮA)
        // =========================================================
        DrawRectangle(380, 15, 300, 200, GetColor(0x383838ff));
        DrawText("AUDIO EFFECTS (HIỆU ỨNG)", 395, 30, 16, LIGHTGRAY);
        
        bool oEcho = fxEcho, oRev = fxReverb, oDist = fxDistort, oTrem = fxTremolo;
        GuiCheckBox((Rectangle){ 395, 65, 20, 20 }, "Echo (Delay 50ms)", &fxEcho);
        GuiCheckBox((Rectangle){ 395, 95, 20, 20 }, "Reverb (Room 0.6)", &fxReverb);
        GuiCheckBox((Rectangle){ 395, 125, 20, 20 }, "Distortion (Hard Clip)", &fxDistort);
        GuiCheckBox((Rectangle){ 395, 155, 20, 20 }, "Tremolo (AM 5Hz)", &fxTremolo);
        if (oEcho != fxEcho || oRev != fxReverb || oDist != fxDistort || oTrem != fxTremolo) signalChanged = true;

        // Các nút bấm Phát / Dừng / Lưu
        if (GuiButton((Rectangle){ 380, 230, 145, 40 }, "PLAY AUDIO")) PlaySignalWithRaylib(currentSignal, activeSound);
        if (GuiButton((Rectangle){ 535, 230, 145, 40 }, "STOP")) if (IsSoundPlaying(activeSound)) StopSound(activeSound);
        if (GuiButton((Rectangle){ 380, 280, 300, 40 }, "EXPORT TO 'output.wav'")) {
            audioFile.save(currentSignal, "output_demo.wav");
        }

        // =========================================================
        // PANEL 3: PHÂN TÍCH PHỔ TẦN SỐ DFT (BÊN PHẢI)
        // =========================================================
        DrawRectangle(695, 15, 390, 305, GetColor(0x1a1a1aff));
        DrawRectangleLines(695, 15, 390, 305, DARKGRAY);
        DrawText("FREQUENCY SPECTRUM (DFT)", 710, 25, 14, GREEN);
        
        // Vẽ biểu đồ Bar Chart cho Phổ tần số
        for (int k = 0; k < 15; k++) {
            int barHeight = min((int)(spectrumBins[k] * 200), 200); // Giới hạn chiều cao
            int barX = 710 + (k * 24);
            int barY = 300 - barHeight;
            DrawRectangle(barX, barY, 18, barHeight, MAROON);
            DrawText(TextFormat("%d", (k+1)*100), barX, 305, 10, LIGHTGRAY);
        }

        // Thông số Real-time
        DrawText(TextFormat("Peak Amp: %.4f", currentSignal.getPeakAmplitude()), 710, 340, 14, LIGHTGRAY);
        DrawText(TextFormat("RMS Value: %.4f", currentSignal.getRMS()), 710, 365, 14, LIGHTGRAY);
        DrawText(TextFormat("Energy (dBFS): %.1f dB", currentSignal.getdBFS()), 710, 390, 14, LIGHTGRAY);

        // =========================================================
        // PANEL 4: OSCILLOSCOPE (ĐỒ THỊ SÓNG) - CUỐI MÀN HÌNH
        // =========================================================
        int gX = 15, gY = 450, gW = screenWidth - 30, gH = 285, mY = gY + gH / 2;
        DrawRectangle(gX, gY, gW, gH, GetColor(0x1a1a1aff));
        DrawRectangleLines(gX, gY, gW, gH, GRAY);
        DrawLine(gX, mY, gX + gW, mY, DARKGRAY);
        DrawText("REAL-TIME OSCILLOSCOPE", gX + 10, gY + 10, 14, GREEN);

        const auto& samples = currentSignal.getSamples();
        if (!samples.empty()) {
            size_t maxPts = 1000;
            size_t step = (samples.size() > maxPts) ? samples.size() / maxPts : 1;
            for (size_t i = 0; i < samples.size() - step && i < maxPts * step; i += step) {
                int x1 = gX + (int)((float)i / (maxPts * step) * gW);
                int y1 = mY - (int)(samples[i] * (gH / 2.2));
                int x2 = gX + (int)((float)(i + step) / (maxPts * step) * gW);
                int y2 = mY - (int)(samples[i + step] * (gH / 2.2));
                DrawLine(x1, y1, x2, y2, GREEN);
            }
        }

        // =========================================================
        // VẼ COMBOBOX CHỌN SÓNG Ở CUỐI ĐỂ KHÔNG BỊ ĐÈ CHUỘT
        // =========================================================
        int oldWave = waveTypeActive;
        GuiComboBox((Rectangle){ 100, 90, 240, 20 }, "Sine;Square;Triangle;White Noise;DTMF (770+1336Hz)", &waveTypeActive);
        if (waveTypeActive != oldWave) signalChanged = true;

        // =========================================================
        // XỬ LÝ CHUỖI TÍN HIỆU (DSP CHAIN) KHI CÓ THAY ĐỔI
        // =========================================================
        if (signalChanged || forceRecalculate) {
            // 1. Tạo sóng gốc (Generator / DTMF)
            if (waveTypeActive == 0) currentSignal = AudioGenerator::generateSine(frequency, 2.0);
            else if (waveTypeActive == 1) currentSignal = AudioGenerator::generateSquare(frequency, 2.0);
            else if (waveTypeActive == 2) currentSignal = AudioGenerator::generateTriangle(frequency, 2.0);
            else if (waveTypeActive == 3) currentSignal = AudioGenerator::generateWhiteNoise(2.0, 0.5);
            else if (waveTypeActive == 4) currentSignal = AudioGenerator::generateDTMF(770, 1336, 2.0); // Phím 5

            // 2. Trộn tín hiệu (Mix)
            if (doMixDrum) {
                AudioSignal drum = AudioGenerator::generateWhiteNoise(2.0, 0.3);
                currentSignal = processor.mix(currentSignal, drum, 0.7);
            }

            // 3. Lọc tín hiệu (Filters)
            if (filterType == 1) { LowPassFilter lpf(cutoffFreq, 44100); lpf.apply(currentSignal); }
            else if (filterType == 2) { HighPassFilter hpf(cutoffFreq, 44100); hpf.apply(currentSignal); }
            else if (filterType == 3) { BandPassFilter bpf(cutoffFreq, cutoffFreq + 1000.0, 44100); bpf.apply(currentSignal); }

            // 4. Các hiệu ứng (Effects)
            if (fxEcho) effect.echo(currentSignal, 0.05, 0.45);
            if (fxReverb) effect.reverb(currentSignal, 0.6);
            if (fxDistort) effect.distort(currentSignal, 0.4);
            if (fxTremolo) effect.tremolo(currentSignal, 5.0, 0.6);

            // 5. Envelope & Khuyếch đại
            if (amplifyFactor != 1.0f) processor.amplify(currentSignal, amplifyFactor);
            if (doFadeInOut) {
                processor.fadeIn(currentSignal, 2000);
                processor.fadeOut(currentSignal, 2000);
            }
            if (doNormalize) processor.normalize(currentSignal);

            // 6. Tính toán lại Phổ tần số (DFT) cho Bar Chart
            const auto& s = currentSignal.getSamples();
            int N = min((int)s.size(), 2048);
            for (int k = 1; k <= 15; k++) {
                double re = 0, im = 0;
                for (int n = 0; n < N; n++) {
                    double angle = 2.0 * M_PI * k * n / N;
                    re += s[n] * cos(angle);
                    im -= s[n] * sin(angle);
                }
                spectrumBins[k-1] = sqrt(re * re + im * im) / N * 2.0;
            }

            // 7. Cập nhật âm thanh ra loa nếu đang phát
            if (IsSoundPlaying(activeSound)) PlaySignalWithRaylib(currentSignal, activeSound);
            forceRecalculate = false;
        }

        EndDrawing();
    }

    if (activeSound.frameCount > 0) UnloadSound(activeSound);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}