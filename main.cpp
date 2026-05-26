#define _USE_MATH_DEFINES
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdlib.h>

#include "AudioSignal.h"
#include "AudioComponents.h"
#include "AudioFile.h"

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
    wave.frameCount = samples.size() / sig.getNumChannels();
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
    const int screenWidth = 1600;
    const int screenHeight = 1000;
    InitWindow(screenWidth, screenHeight, "DIGITAL AUDIO WORKSTATION - GUI");
    InitAudioDevice();

    // =========================================================
    // NẠP FONT CHỮ TIẾNG VIỆT 
    // =========================================================
    int codepoints[500];
    int count = 0;
    for (int i = 32; i < 127; i++) codepoints[count++] = i;         // Bảng mã ASCII cơ bản (Tiếng Anh, số, dấu)
    for (int i = 0x00C0; i <= 0x01B0; i++) codepoints[count++] = i; // Vùng Unicode Tiếng Việt 1
    for (int i = 0x1EA0; i <= 0x1EF9; i++) codepoints[count++] = i; // Vùng Unicode Tiếng Việt 2 (Dấu nặng, ngã...)

    // Tải font từ file vừa copy, kích thước mặc định 18
    Font myFont = LoadFontEx("segoeui.ttf", 18, codepoints, count);
    
    // Bật chế độ lọc khử răng cưa (Antialiasing) giúp chữ siêu nét và mịn màng
    SetTextureFilter(myFont.texture, TEXTURE_FILTER_BILINEAR); 

    // Áp dụng font mới này cho TOÀN BỘ giao diện của Raygui (Slider, Checkbox, Button...)
    GuiSetFont(myFont);

    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(WHITE));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(WHITE));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(WHITE));

    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(BLACK));
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToInt(BLACK));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, ColorToInt(BLACK));

    GuiSetStyle(COMBOBOX, TEXT_COLOR_NORMAL, ColorToInt(BLACK));
    GuiSetStyle(COMBOBOX, TEXT_COLOR_FOCUSED, ColorToInt(BLACK));
    GuiSetStyle(COMBOBOX, TEXT_COLOR_PRESSED, ColorToInt(BLACK));

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

    // quản lý file kéo thả
    AudioSignal customSignal;       // Dùng để lưu trữ âm thanh từ file
    bool hasCustomFile = false;     // Cờ đánh dấu xem đã có file nào được kéo vào chưa

    while (!WindowShouldClose()) {
        bool signalChanged = false;

        BeginDrawing();
        ClearBackground(GetColor(0x2b2b2bff)); // Nền Dark Mode chuyên nghiệp

        // =========================================================
        // XỬ LÝ KÉO THẢ FILE ÂM THANH (.WAV)
        // =========================================================
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            
            if (droppedFiles.count > 0 && IsFileExtension(droppedFiles.paths[0], ".wav")) {
                Wave loadedWave = LoadWave(droppedFiles.paths[0]);
                WaveFormat(&loadedWave, loadedWave.sampleRate, 32, loadedWave.channels);
                
                float* waveData = (float*)loadedWave.data;
                vector<double> newSamples(loadedWave.frameCount * loadedWave.channels);
                for (unsigned int i = 0; i < loadedWave.frameCount * loadedWave.channels; i++) {
                    newSamples[i] = waveData[i];
                }
                
                // Lưu dữ liệu vào biến customSignal riêng biệt
                customSignal = AudioSignal(newSamples, loadedWave.sampleRate, loadedWave.channels, 32);
                customSignal.setName(GetFileName(droppedFiles.paths[0]));
                
                hasCustomFile = true; // Xác nhận đã có file
                waveTypeActive = 5;   // Tự động chuyển ComboBox sang chế độ Custom File (vị trí số 5)
                forceRecalculate = true;
                
                UnloadWave(loadedWave);
            }
            UnloadDroppedFiles(droppedFiles);
        }

        // =========================================================
        // PANEL 1: ĐIỀU KHIỂN CHÍNH (BÊN TRÁI)
        // =========================================================
        DrawRectangle(15, 15, 350, 420, GetColor(0x383838ff));
        DrawTextEx(myFont, "SIGNAL GENERATOR", {30, 30}, 16, 1.0f, WHITE);
        
        // Chọn sóng & Tần số
        float oldFreq = frequency;
        GuiSlider((Rectangle){ 100, 60, 240, 20 }, "Freq (Hz)", TextFormat("%.0f", frequency), &frequency, 50.0f, 2000.0f);
        if (abs(frequency - oldFreq) > 1.0f) signalChanged = true;

        // Bộ lọc số (Digital Filter)
        DrawTextEx(myFont, "DIGITAL FILTERS", {30, 130}, 16, 1.0f, WHITE);
        float oldCutoff = cutoffFreq;
        GuiSlider((Rectangle){ 100, 160, 240, 20 }, "Cutoff", TextFormat("%.0f Hz", cutoffFreq), &cutoffFreq, 100.0f, 3000.0f);
        if (abs(cutoffFreq - oldCutoff) > 10.0f) signalChanged = true;

        int oldFilter = filterType;
        GuiComboBox((Rectangle){ 100, 190, 240, 20 }, "None;Low-Pass Filter;High-Pass Filter;Band-Pass Filter", &filterType);
        if (filterType != oldFilter) signalChanged = true;

        // Các Modify khác
        DrawTextEx(myFont, "MODIFIERS & 8. MIXING", {30, 240}, 16, 1.0f, WHITE);
        float oldAmp = amplifyFactor;
        GuiSlider((Rectangle){ 100, 270, 240, 20 }, "Amplify", TextFormat("x%.1f", amplifyFactor), &amplifyFactor, 0.1f, 3.0f);
        if (abs(amplifyFactor - oldAmp) > 0.1f) signalChanged = true;

        bool oldNorm = doNormalize; GuiCheckBox((Rectangle){ 30, 310, 20, 20 }, "Normalize (Chuẩn hoá)", &doNormalize);
        bool oldFade = doFadeInOut; GuiCheckBox((Rectangle){ 30, 340, 20, 20 }, "Fade In / Fade Out (2000 samples)", &doFadeInOut);
        bool oldMix  = doMixDrum;   GuiCheckBox((Rectangle){ 30, 370, 20, 20 }, "Mix with Drum Noise (Trộn tín hiệu)", &doMixDrum);
        if (doNormalize != oldNorm || doFadeInOut != oldFade || doMixDrum != oldMix) signalChanged = true;

        // =========================================================
        // PANEL 2: HIỆU ỨNG & CÔNG CỤ (GIỮA)
        // =========================================================
        DrawRectangle(380, 15, 300, 200, GetColor(0x383838ff));
        DrawTextEx(myFont, u8"AUDIO EFFECTS (HIỆU ỨNG)", {395, 30}, 16, 1.0f, WHITE);
        
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
        int specX = 695;
        int specY = 15;
        int specW = screenWidth - specX - 15; // Tự động kéo dài sát lề phải
        int specH = 420;                      // Tự động cao bằng Panel 1

        DrawRectangle(specX, specY, specW, specH, GetColor(0x1a1a1aff));
        DrawRectangleLines(specX, specY, specW, specH, DARKGRAY);
        DrawTextEx(myFont, "FREQUENCY SPECTRUM (DFT)", {(float)specX + 15, (float)specY + 15}, 14, 1.0f, GREEN);
        
        int numBins = 15;
        float stepX = (float)(specW - 30) / numBins;
        float barW = stepX * 0.7f;
        float maxBarH = specH - 120;

        for (int k = 0; k < numBins; k++) {
            int barHeight = min((int)(spectrumBins[k] * maxBarH * 1.5), (int)maxBarH); 
            int barX = specX + 15 + (int)(k * stepX);
            int barY = specY + specH - 80 - barHeight;
            
            DrawRectangle(barX, barY, (int)barW, barHeight, MAROON);
            DrawTextEx(myFont, TextFormat("%d", (k+1)*100), {(float)barX, (float)(specY + specH - 75)}, 12, 1.0f, WHITE);
        }

        DrawLine(specX, specY + specH - 50, specX + specW, specY + specH - 50, DARKGRAY);

        DrawTextEx(myFont, TextFormat("Peak Amp: %.4f", currentSignal.getPeakAmplitude()), {(float)specX + 20, (float)specY + specH - 30}, 14, 1.0f, WHITE);
        DrawTextEx(myFont, TextFormat("RMS Value: %.4f", currentSignal.getRMS()), {(float)specX + 250, (float)specY + specH - 30}, 14, 1.0f, WHITE);
        DrawTextEx(myFont, TextFormat("Energy: %.1f dBFS", currentSignal.getdBFS()), {(float)specX + 480, (float)specY + specH - 30}, 14, 1.0f, WHITE);
        
        // =========================================================
        // PANEL 4: OSCILLOSCOPE (ĐỒ THỊ SÓNG) - CUỐI MÀN HÌNH
        // =========================================================
        int gX = 15, gY = 450, gW = screenWidth - 30, gH = screenHeight - gY - 40, mY = gY + gH / 2;
        DrawRectangle(gX, gY, gW, gH, GetColor(0x1a1a1aff));
        DrawRectangleLines(gX, gY, gW, gH, GRAY);
        DrawLine(gX, mY, gX + gW, mY, DARKGRAY);
        DrawTextEx(myFont, "REAL-TIME OSCILLOSCOPE", {(float)gX + 10, (float)gY + 10}, 14, 1.0f, GREEN);

        const auto& samples = currentSignal.getSamples();
        if (!samples.empty()) {
            size_t maxPts = 1000;
            size_t step = (samples.size() > maxPts) ? samples.size() / maxPts : 1;

            BeginScissorMode(gX, gY, gW, gH);

            for (size_t i = 0; i < samples.size() - step && i < maxPts * step; i += step) {
                int x1 = gX + (int)((float)i / (maxPts * step) * gW);
                int y1 = mY - (int)(samples[i] * (gH / 6.0));
                int x2 = gX + (int)((float)(i + step) / (maxPts * step) * gW);
                int y2 = mY - (int)(samples[i + step] * (gH / 6.0));
                DrawLine(x1, y1, x2, y2, GREEN);
            }
            EndScissorMode();
        }

        // =========================================================
        // VẼ COMBOBOX CHỌN SÓNG Ở CUỐI ĐỂ KHÔNG BỊ ĐÈ CHUỘT
        // =========================================================
        int oldWave = waveTypeActive;
        GuiComboBox((Rectangle){ 100, 90, 240, 20 }, "Sine;Square;Triangle;White Noise;DTMF;Custom File", &waveTypeActive);
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
            else if (waveTypeActive == 5 && hasCustomFile) {
                // CHẾ ĐỘ FILE BÊN NGOÀI
                if (hasCustomFile) {
                    currentSignal = customSignal; // Gán tín hiệu bằng file đã kéo vào
                } else {
                    // Nếu user cố tình chọn Custom File nhưng chưa kéo thả gì, tạo một sóng im lặng
                    currentSignal = AudioGenerator::generateSine(0, 2.0, 0.0); 
                }
            }
            // 2. Trộn tín hiệu (Mix)
            if (doMixDrum) {
                // Tự tạo nhiễu trắng (White Noise) có cùng chiều dài và cùng số kênh (Mono/Stereo) với tín hiệu gốc
                vector<double> noiseSamples = currentSignal.getSamples();
                for (auto& s : noiseSamples) {
                    // Tạo nhiễu ngẫu nhiên với biên độ 0.3
                    s = 0.3 * (2.0 * (rand() / (double)RAND_MAX) - 1.0);
                }
                
                // Đóng gói lại thành một AudioSignal "tiếng nhiễu"
                AudioSignal drum(noiseSamples, currentSignal.getSampleRate(), currentSignal.getNumChannels(), currentSignal.getBitDepth());
                
                // Trộn: 70% âm thanh gốc, 30% tiếng nhiễu
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