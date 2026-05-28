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

// Hàm hỗ trợ vẽ dấu tích tùy chỉnh cho ô CheckBox thêm phần sắc nét
void DrawCheckmark(Rectangle bounds)
{
    DrawRectangle(bounds.x + 4, bounds.y + 4, bounds.width - 8, bounds.height - 8, GetColor(0x383838ff));
    Vector2 p1 = {bounds.x + 6, bounds.y + bounds.height / 2 + 2};
    Vector2 p2 = {bounds.x + bounds.width / 2 - 2, bounds.y + bounds.height - 4};
    Vector2 p3 = {bounds.x + bounds.width - 4, bounds.y + 6};
    DrawLineEx(p1, p2, 3.5f, GREEN);
    DrawLineEx(p2, p3, 3.5f, GREEN);
}

// Hàm cầu nối cập nhật luồng phát âm thanh thời gian thực ra loa/tai nghe phần cứng
void PlaySignalWithRaylib(const AudioSignal &sig, Sound &currentSound)
{
    if (currentSound.frameCount > 0)
    {
        StopSound(currentSound);
        UnloadSound(currentSound);
    }
    const auto &samples = sig.getSamples();
    if (samples.empty())
        return;

    float *floatData = (float *)MemAlloc(samples.size() * sizeof(float));
    for (size_t i = 0; i < samples.size(); i++)
        floatData[i] = static_cast<float>(samples[i]);

    Wave wave = {0};
    wave.frameCount = samples.size() / sig.getNumChannels();
    wave.sampleRate = sig.getSampleRate();
    wave.sampleSize = 32;
    wave.channels = sig.getNumChannels();
    wave.data = floatData;

    currentSound = LoadSoundFromWave(wave);
    PlaySound(currentSound);
    UnloadWave(wave);
}

// Chuyển tín hiệu interleaved (nhiều kênh Stereo) về mono (trung bình cộng các kênh)
AudioSignal convertToMono(const AudioSignal &sig)
{
    if (sig.getNumChannels() == 1)
        return sig;
    const auto &samples = sig.getSamples();
    int ch = sig.getNumChannels();
    vector<double> mono(samples.size() / ch);
    for (size_t i = 0; i < mono.size(); ++i)
    {
        double sum = 0.0;
        for (int c = 0; c < ch; ++c)
            sum += samples[i * ch + c];
        mono[i] = sum / ch;
    }
    return AudioSignal(mono, sig.getSampleRate(), 1, sig.getBitDepth());
}

int main()
{
    // 1. KHỞI TẠO CỬA SỔ ỨNG DỤNG & THIẾT BỊ ÂM THANH
    const int screenWidth = 1600;
    const int screenHeight = 1000;
    InitWindow(screenWidth, screenHeight, "DIGITAL AUDIO WORKSTATION - GUI");
    InitAudioDevice();

    // =========================================================
    // NẠP FONT CHỮ VÀ PHÂN VÙNG TIẾNG VIỆT UNICODE
    // =========================================================
    int codepoints[500];
    int count = 0;
    for (int i = 32; i < 127; i++)
        codepoints[count++] = i;
    for (int i = 0x00C0; i <= 0x01B0; i++)
        codepoints[count++] = i;
    for (int i = 0x1EA0; i <= 0x1EF9; i++)
        codepoints[count++] = i;

    Font myFont = LoadFontEx("segoeui.ttf", 18, codepoints, count);
    SetTextureFilter(myFont.texture, TEXTURE_FILTER_BILINEAR); // Chống răng cưa font
    GuiSetFont(myFont);

    SetTargetFPS(60);

    // Đặt cấu hình giao diện tối (Dark Mode Style) cho Raygui
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

    // Khởi tạo các Core Class xử lý hạ tầng kỹ thuật DSP
    AudioProcessor processor;
    AudioEffect effect(0.5, 100.0);
    AudioFile audioFile("output.wav");
    Sound activeSound = {0};

    // --- BIẾN ĐIỀU KHIỂN TRẠNG THÁI HỆ THỐNG ---
    float frequency = 440.0f;
    int waveTypeActive = 0; // 0: Sine, 1: Square, 2: Triangle, 3: Noise, 4: DTMF, 5: Custom
    float amplifyFactor = 1.0f;
    bool doNormalize = false;
    bool doInvertPhase = false; // Biến trạng thái đảo pha bổ sung
    bool doPhaseTest = false;   // CẬP NHẬT: Biến trạng thái thử nghiệm triệt tiêu pha
    int filterType = 0;         // 0: None, 1: LPF, 2: HPF, 3: BPF
    float cutoffFreq = 500.0f;

    bool fxEcho = false, fxReverb = false, fxChorus = false, fxDistort = false, fxTremolo = false;
    bool doMixDrum = false;
    bool doFadeInOut = false;

    // Mảng lưu trữ phổ tần số rời rạc (2048 bins ứng với nửa đối xứng Nyquist của khối 4096)
    vector<double> spectrumBins(2048, 0.0);

    // Khởi tạo tín hiệu mặc định ban đầu (Sóng Sin 440Hz thời lượng 2 giây)
    AudioSignal currentSignal = AudioGenerator::generateSine(frequency, 2.0);
    bool forceRecalculate = true;

    AudioSignal customSignal;
    bool hasCustomFile = false;

    while (!WindowShouldClose())
    {
        bool signalChanged = false;

        BeginDrawing();
        ClearBackground(GetColor(0x2b2b2bff)); // Nền Dark mode màu xám chuyên nghiệp

        // =========================================================
        // XỬ LÝ SỰ KIỆN KÉO THẢ TỆP TIN NGOẠI VI (.WAV)
        // =========================================================
        // SỬA LẠI TRONG KHỐI IsFileDropped() ĐỂ ĐO ĐẠC CHÍNH XÁC
        if (IsFileDropped())
        {
            FilePathList droppedFiles = LoadDroppedFiles();
            if (droppedFiles.count > 0 && IsFileExtension(droppedFiles.paths[0], ".wav"))
            {
                Wave loadedWave = LoadWave(droppedFiles.paths[0]);

                // GIỮ NGUYÊN ĐỊNH DẠNG 16-BIT GỐC CỦA FILE POP
                WaveFormat(&loadedWave, loadedWave.sampleRate, 16, loadedWave.channels);

                short *waveData = (short *)loadedWave.data; // Đọc dạng short nguyên bản
                vector<double> newSamples(loadedWave.frameCount * loadedWave.channels);
                for (unsigned int i = 0; i < loadedWave.frameCount * loadedWave.channels; i++)
                {
                    // Chuẩn hóa thủ công về khoảng [-1.0, 1.0] chuẩn DSP
                    newSamples[i] = (double)waveData[i] / 32768.0;
                }

                customSignal = AudioSignal(newSamples, loadedWave.sampleRate, loadedWave.channels, 16);
                customSignal.setName(GetFileName(droppedFiles.paths[0]));
                hasCustomFile = true;
                waveTypeActive = 5;
                forceRecalculate = true;
                UnloadWave(loadedWave);
            }
            UnloadDroppedFiles(droppedFiles);
        }

        // =========================================================
        // PANEL 1: BẢNG SINH TÍN HIỆU & ĐIỀU BIẾN BIÊN ĐỘ (NÂNG LÊN 480PX ĐỂ VỪA Ô MIX PHA)
        // =========================================================
        DrawRectangle(15, 15, 350, 480, GetColor(0x383838ff)); // Nâng lên 480 để chèn thêm checkbox
        DrawTextEx(myFont, "SIGNAL GENERATOR", {30, 30}, 16, 1.0f, WHITE);

        float oldFreq = frequency;
        GuiSlider((Rectangle){100, 60, 240, 20}, "Freq (Hz)", TextFormat("%.0f", frequency), &frequency, 50.0f, 2000.0f);
        if (abs(frequency - oldFreq) > 1.0f)
            signalChanged = true;

        DrawTextEx(myFont, "DIGITAL FILTERS", {30, 130}, 16, 1.0f, WHITE);
        float oldCutoff = cutoffFreq;
        GuiSlider((Rectangle){100, 160, 240, 20}, "Cutoff", TextFormat("%.0f Hz", cutoffFreq), &cutoffFreq, 100.0f, 3000.0f);
        if (abs(cutoffFreq - oldCutoff) > 10.0f)
            signalChanged = true;

        int oldFilter = filterType;
        GuiComboBox((Rectangle){100, 190, 240, 20}, "None;Low-Pass Filter;High-Pass Filter;Band-Pass Filter", &filterType);
        if (filterType != oldFilter)
            signalChanged = true;

        DrawTextEx(myFont, "MODIFIERS & MIXING", {30, 240}, 16, 1.0f, WHITE);
        float oldAmp = amplifyFactor;
        GuiSlider((Rectangle){100, 270, 240, 20}, "Amplify", TextFormat("x%.1f", amplifyFactor), &amplifyFactor, 0.1f, 3.0f);
        if (abs(amplifyFactor - oldAmp) > 0.1f)
            signalChanged = true;

        bool oldNorm = doNormalize;
        GuiCheckBox((Rectangle){30, 310, 20, 20}, "Normalize (Chuẩn hoá)", &doNormalize);
        bool oldInvert = doInvertPhase;
        GuiCheckBox((Rectangle){30, 340, 20, 20}, "Invert Phase (Đảo pha)", &doInvertPhase);
        bool oldFade = doFadeInOut;
        GuiCheckBox((Rectangle){30, 370, 20, 20}, "Fade In / Fade Out (2000 samples)", &doFadeInOut);
        bool oldMix = doMixDrum;
        GuiCheckBox((Rectangle){30, 410, 20, 20}, "Mix with White Noise (Trộn tín hiệu)", &doMixDrum); // Hạ Y xuống 410
        bool oldTest = doPhaseTest;
        GuiCheckBox((Rectangle){30, 440, 20, 20}, "Test Phase Cancel (Trộn pha)", &doPhaseTest); // CẬP NHẬT CHECKBOX MỚI Y=440

        if (doNormalize != oldNorm || doInvertPhase != oldInvert || doFadeInOut != oldFade || doMixDrum != oldMix || doPhaseTest != oldTest)
            signalChanged = true;

        if (doNormalize)
            DrawCheckmark((Rectangle){30, 310, 20, 20});
        if (doInvertPhase)
            DrawCheckmark((Rectangle){30, 340, 20, 20});
        if (doFadeInOut)
            DrawCheckmark((Rectangle){30, 370, 20, 20});
        if (doMixDrum)
            DrawCheckmark((Rectangle){30, 410, 20, 20});
        if (doPhaseTest)
            DrawCheckmark((Rectangle){30, 440, 20, 20}); // Vẽ tích xanh cho chức năng mới

        // =========================================================
        // PANEL 2: BẢNG ĐIỀU KHIỂN HIỆU ỨNG KHÔNG GIAN KỸ XẢO
        // =========================================================
        DrawRectangle(380, 15, 300, 230, GetColor(0x383838ff));
        DrawTextEx(myFont, "AUDIO EFFECTS (HIỆU ỨNG)", {395, 30}, 16, 1.0f, WHITE);

        bool oEcho = fxEcho, oRev = fxReverb, oCho = fxChorus, oDist = fxDistort, oTrem = fxTremolo;
        GuiCheckBox((Rectangle){395, 65, 20, 20}, "Echo (Delay 50ms)", &fxEcho);
        GuiCheckBox((Rectangle){395, 95, 20, 20}, "Reverb (Room 0.6)", &fxReverb);
        GuiCheckBox((Rectangle){395, 125, 20, 20}, "Chorus Effect (AM)", &fxChorus);
        GuiCheckBox((Rectangle){395, 155, 20, 20}, "Distortion (Hard Clip)", &fxDistort);
        GuiCheckBox((Rectangle){395, 185, 20, 20}, "Tremolo (AM 5Hz)", &fxTremolo);
        if (oEcho != fxEcho || oRev != fxReverb || oCho != fxChorus || oDist != fxDistort || oTrem != fxTremolo)
            signalChanged = true;

        if (fxEcho)
            DrawCheckmark((Rectangle){395, 65, 20, 20});
        if (fxReverb)
            DrawCheckmark((Rectangle){395, 95, 20, 20});
        if (fxChorus)
            DrawCheckmark((Rectangle){395, 125, 20, 20});
        if (fxDistort)
            DrawCheckmark((Rectangle){395, 155, 20, 20});
        if (fxTremolo)
            DrawCheckmark((Rectangle){395, 185, 20, 20});

        if (GuiButton((Rectangle){380, 260, 145, 40}, "PLAY AUDIO"))
            PlaySignalWithRaylib(currentSignal, activeSound);
        if (GuiButton((Rectangle){535, 260, 145, 40}, "STOP"))
            if (IsSoundPlaying(activeSound))
                StopSound(activeSound);
        if (GuiButton((Rectangle){380, 310, 300, 40}, "EXPORT TO 'output.wav'"))
        {
            audioFile.save(currentSignal, "output_demo.wav");
        }

        // =========================================================
        // PANEL 3: BỘ PHÂN TÍCH PHỔ TẦN SỐ VÀ MAPPING HẠ TẦNG (ĐỒNG BỘ ĐỘ CAO THEO PANEL 1)
        // =========================================================
        int specX = 695, specY = 15;
        int specW = screenWidth - specX - 15;
        int specH = 480; // Tăng từ 450 lên 480 để đồng bộ song song tuyệt đối với chiều cao mới của Panel 1
        DrawRectangle(specX, specY, specW, specH, GetColor(0x1a1a1aff));
        DrawRectangleLines(specX, specY, specW, specH, DARKGRAY);
        DrawTextEx(myFont, "FREQUENCY SPECTRUM (DFT)", {(float)specX + 15, (float)specY + 15}, 14, 1.0f, GREEN);

        int numBins = 15;
        float stepX = (float)(specW - 30) / numBins;
        float barW = stepX * 0.7f;
        float maxBarH = specH - 120;
        int sampleRate = currentSignal.getSampleRate();
        double binSize = (double)sampleRate / 4096.0;

        for (int k = 0; k < numBins; ++k)
        {
            int targetFreq = (k + 1) * 100;
            int dftIndex = (int)(targetFreq / binSize + 0.5);

            if (dftIndex < 0)
                dftIndex = 0;
            if (dftIndex >= (int)spectrumBins.size())
                dftIndex = (int)spectrumBins.size() - 1;

            double magnitude = spectrumBins[dftIndex];
            int barHeight = min((int)(magnitude * maxBarH * 15.0), (int)maxBarH);
            int barX = specX + 15 + (int)(k * stepX);
            int barY = specY + specH - 80 - barHeight;

            DrawRectangle(barX, barY, (int)barW, barHeight, MAROON);
            DrawTextEx(myFont, TextFormat("%d", targetFreq), {(float)barX, (float)(specY + specH - 75)}, 12, 1.0f, WHITE);
        }

        DrawLine(specX, specY + specH - 50, specX + specW, specY + specH - 50, DARKGRAY);
        DrawTextEx(myFont, TextFormat("Peak Amp: %.4f", currentSignal.getPeakAmplitude()), {(float)specX + 20, (float)specY + specH - 30}, 14, 1.0f, WHITE);
        DrawTextEx(myFont, TextFormat("RMS Value: %.4f", currentSignal.getRMS()), {(float)specX + 250, (float)specY + specH - 30}, 14, 1.0f, WHITE);
        DrawTextEx(myFont, TextFormat("Energy: %.1f dBFS", currentSignal.getdBFS()), {(float)specX + 480, (float)specY + specH - 30}, 14, 1.0f, WHITE);

        // =========================================================
        // PANEL 4: DAO ĐỘNG KÝ REAL-TIME OSCILLOSCOPE (TỊNH TIẾN XUỐNG 510 TRANH CHỒNG LÊN TRÊN)
        // =========================================================
        AudioSignal displayMono = convertToMono(currentSignal);
        const auto &waveSamples = displayMono.getSamples();

        int gX = 15, gY = 510, gW = screenWidth - 30, gH = screenHeight - gY - 20, mY = gY + gH / 2; // Dịch gY từ 480 xuống 510 để hở khoảng trống thông thoáng với lề Panel trên
        DrawRectangle(gX, gY, gW, gH, GetColor(0x1a1a1aff));
        DrawRectangleLines(gX, gY, gW, gH, GRAY);
        DrawLine(gX, mY, gX + gW, mY, DARKGRAY);
        DrawTextEx(myFont, "REAL-TIME OSCILLOSCOPE", {(float)gX + 10, (float)gY + 10}, 14, 1.0f, GREEN);

        if (!waveSamples.empty())
        {
            size_t maxPts = 1000;
            size_t step = (waveSamples.size() > maxPts) ? waveSamples.size() / maxPts : 1;
            BeginScissorMode(gX, gY, gW, gH);
            for (size_t i = 0; i < waveSamples.size() - step && i < maxPts * step; i += step)
            {
                int x1 = gX + (int)((float)i / (maxPts * step) * gW);
                int y1 = mY - (int)(waveSamples[i] * (gH / 6.0));
                int x2 = gX + (int)((float)(i + step) / (maxPts * step) * gW);
                int y2 = mY - (int)(waveSamples[i + step] * (gH / 6.0));
                DrawLine(x1, y1, x2, y2, GREEN);
            }
            EndScissorMode();
        }

        // COMBOBOX LỰA CHỌN DẠNG SÓNG (Đặt cuối luồng vẽ)
        int oldWave = waveTypeActive;
        GuiComboBox((Rectangle){100, 90, 240, 20}, "Sine;Square;Triangle;White Noise;DTMF;Custom File", &waveTypeActive);
        if (waveTypeActive != oldWave)
            signalChanged = true;

        // =========================================================
        // ĐỘNG CƠ XỬ LÝ LÕI TOÁN HỌC (DSP SYSTEM CHAIN)
        // =========================================================
        if (signalChanged)
        {
            forceRecalculate = true;
            if (IsSoundPlaying(activeSound))
            {
                PlaySignalWithRaylib(currentSignal, activeSound);
            }
        }

        if (forceRecalculate)
        {
            // Bước 1: Máy phát tín hiệu nền (Generator Stage)
            if (waveTypeActive == 0)
                currentSignal = AudioGenerator::generateSine(frequency, 2.0);
            else if (waveTypeActive == 1)
                currentSignal = AudioGenerator::generateSquare(frequency, 2.0);
            else if (waveTypeActive == 2)
                currentSignal = AudioGenerator::generateTriangle(frequency, 2.0);
            else if (waveTypeActive == 3)
                currentSignal = AudioGenerator::generateWhiteNoise(2.0, 0.5);
            else if (waveTypeActive == 4)
                currentSignal = AudioGenerator::generateDTMF(770, 1336, 2.0);
            else if (waveTypeActive == 5 && hasCustomFile)
            {
                currentSignal = customSignal;
                currentSignal.setName(customSignal.getName());
            }
            else
            {
                currentSignal = AudioGenerator::generateSine(0, 2.0, 0.0);
            }

            // Bước 2: Khối hòa trộn nhiễu (Mixing Stage)
            if (doMixDrum)
            {
                vector<double> noiseSamples = currentSignal.getSamples();
                srand(42);
                for (auto &s : noiseSamples)
                    s = 0.3 * (2.0 * (rand() / (double)RAND_MAX) - 1.0);
                AudioSignal drum(noiseSamples, currentSignal.getSampleRate(), currentSignal.getNumChannels(), currentSignal.getBitDepth());
                currentSignal = processor.mix(currentSignal, drum, 0.7);
            }

            // Bước 3: Bộ lọc số (Digital Filtering Stage)
            if (filterType == 1)
            {
                LowPassFilter lpf(cutoffFreq, 44100);
                lpf.apply(currentSignal);
            }
            else if (filterType == 2)
            {
                HighPassFilter hpf(cutoffFreq, 44100);
                hpf.apply(currentSignal);
            }
            else if (filterType == 3)
            {
                BandPassFilter bpf(cutoffFreq, min(cutoffFreq + 800.0f, 21000.0f), 44100);
                bpf.apply(currentSignal);
            }

            // Bước 4: Hiệu ứng không gian thời gian (Audio Effects Stage)
            if (fxEcho)
                effect.echo(currentSignal, 0.05, 0.45);
            if (fxReverb)
                effect.reverb(currentSignal, 0.6);
            if (fxChorus)
                effect.chorus(currentSignal, 1.5, 0.003);
            if (fxDistort)
                effect.distort(currentSignal, 0.4);
            if (fxTremolo)
                effect.tremolo(currentSignal, 5.0, 0.6);

            // Bước 5: Điều chỉnh biên độ đỉnh và dải động (Master Stage)
            if (amplifyFactor != 1.0f)
                processor.amplify(currentSignal, amplifyFactor);

            if (doInvertPhase)
                processor.invertPhase(currentSignal);

            // CẬP NHẬT BACKEND: Xử lý trộn triệt tiêu pha tuần tự tuyến tính
            if (doPhaseTest)
            {
                AudioSignal inverted = currentSignal;                        // Nhân bản dòng tín hiệu hiện tại
                processor.invertPhase(inverted);                             // Đảo ngược toàn bộ cực biên độ bản sao
                currentSignal = processor.mix(currentSignal, inverted, 0.5); // Trộn tỷ lệ đối xứng 50-50 để triệt tiêu năng lượng về 0
            }

            if (doFadeInOut)
            {
                processor.fadeIn(currentSignal, 2000);
                processor.fadeOut(currentSignal, 2000);
            }
            if (doNormalize)
                processor.normalize(currentSignal);

            // Bước 6: Phân tách và phân tích phổ tần số rời rạc dải rộng đầy đủ (DFT Processing)
            AudioSignal monoForDFT = convertToMono(currentSignal);
            monoForDFT.resizeAndZeroPad(4096);
            const auto &dftData = monoForDFT.getSamples();
            int N_DFT = 4096;

            fill(spectrumBins.begin(), spectrumBins.end(), 0.0);
            for (int k = 0; k < N_DFT / 2; ++k)
            {
                double re = 0.0, im = 0.0;
                for (int n = 0; n < N_DFT; ++n)
                {
                    double window = 0.5 * (1.0 - cos(2.0 * M_PI * n / (N_DFT - 1)));
                    double angle = 2.0 * M_PI * k * n / N_DFT;
                    re += (dftData[n] * window) * cos(angle);
                    im -= (dftData[n] * window) * sin(angle);
                }
                spectrumBins[k] = sqrt(re * re + im * im) / N_DFT * 2.0;
            }

            forceRecalculate = false;
        }

        EndDrawing();
    }

    if (activeSound.frameCount > 0)
        UnloadSound(activeSound);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}