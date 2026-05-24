# Project1 - Hệ Thống Xử Lý Tín Hiệu Âm Thanh Số

## Digital Audio Signal Processing System (C++ OOP)

---

## 📋 Mục Đích Dự Án

Dự án này xây dựng một **hệ thống xử lý tín hiệu âm thanh số (DSP)** hoàn chỉnh bằng C++ với các tính năng:

- 🎵 Tạo các dạng sóng âm thanh khác nhau (sine, square, triangle, noise, DTMF)
- 🔊 Xử lý âm thanh (khuyếch đại, chuẩn hoá, pha đảo, fade in/out)
- 🎚️ Lọc tín hiệu (Low-Pass, High-Pass, Band-Pass filters)
- 🎙️ Hiệu ứng âm thanh (echo, reverb)
- 📊 Phân tích & visualize tín hiệu (waveform, spectrum, statistics)

---

## 🏗️ Kiến Trúc Lớp (Class Architecture)

### 1. **AudioSignal** (`AudioSignal.h`)

Lớp cơ bản đại diện cho tín hiệu âm thanh số.

**Thuộc tính:**

- `samples`: Mảng các mẫu âm thanh (double vector)
- `sampleRate`: Tần số lấy mẫu (Hz) - mặc định 44100 Hz
- `numChannels`: Số kênh âm (1=mono, 2=stereo) - mặc định 1
- `bitDepth`: Độ sâu bit (8/16/24/32 bit) - mặc định 16
- `name`: Tên tín hiệu

**Phương thức chính:**

```cpp
const vector<double>& getSamples() const;        // Lấy mảng mẫu
int getSampleRate() const;                       // Lấy sample rate
int getNumChannels() const;                      // Lấy số kênh
int getBitDepth() const;                         // Lấy bit depth
double getDuration() const;                      // Tính thời lượng (s)
size_t getNumSamples() const;                    // Số lượng mẫu
double getPeakAmplitude() const;                 // Biên độ cực đại
double getRMS() const;                           // RMS (Root Mean Square)
double getdBFS() const;                          // Mức dBFS (dB Full Scale)
void setSamples(const vector<double>& s);       // Đặt mẫu
void addSample(double s);                        // Thêm 1 mẫu
void clear();                                    // Xoá tất cả mẫu
```

---

### 2. **AudioGenerator** (`AudioFile.h`)

Tạo các dạng sóng âm thanh khác nhau.

**Phương thức tĩnh:**

```cpp
// Tạo sóng sin
static AudioSignal generateSine(double freq, double duration,
                                 double amplitude = 0.8, int sr = 44100)

// Tạo sóng vuông
static AudioSignal generateSquare(double freq, double duration,
                                   double amplitude = 0.6, int sr = 44100)

// Tạo sóng tam giác
static AudioSignal generateTriangle(double freq, double duration,
                                     double amplitude = 0.8, int sr = 44100)

// Tạo noise trắng
static AudioSignal generateWhiteNoise(double duration,
                                       double amplitude = 0.3, int sr = 44100)

// Tạo tín hiệu DTMF (Dual-Tone Multi-Frequency)
static AudioSignal generateDTMF(double f1, double f2, double duration,
                                 int sr = 44100)
```

**Ví dụ:**

```cpp
AudioSignal sine440Hz = AudioGenerator::generateSine(440.0, 0.1, 0.8);
// Tạo sóng sin 440 Hz, thời lượng 0.1s, biên độ 0.8
```

---

### 3. **AudioProcessor** (`AudioComponents.h`)

Xử lý & điều chỉnh tín hiệu.

**Phương thức:**

```cpp
// Khuyếch đại tín hiệu
void amplify(AudioSignal& signal, double factor)

// Chuẩn hoá tín hiệu (peak = 1.0)
void normalize(AudioSignal& signal)

// Đảo pha (nhân -1)
void invertPhase(AudioSignal& signal)

// Fade in (tăng dần từ 0 đến 1)
void fadeIn(AudioSignal& signal, int numFadeSamples)

// Fade out (giảm dần từ 1 đến 0)
void fadeOut(AudioSignal& signal, int numFadeSamples)

// Trộn 2 tín hiệu
AudioSignal mix(const AudioSignal& a, const AudioSignal& b,
                 double ratioA = 0.5)
```

---

### 4. **AudioFilter** & Các Bộ Lọc (`AudioComponents.h`)

#### **AudioFilter** (Lớp trừu tượng)

Lớp cơ sở cho tất cả các bộ lọc.

**Thuộc tính:**

- `cutoffFreq`: Tần số cắt
- `order`: Bậc lọc
- `sampleRate`: Tần số lấy mẫu

**Phương thức ảo:**

```cpp
virtual void apply(AudioSignal& signal) = 0;         // Áp dụng lọc
virtual string getFilterType() const = 0;            // Loại lọc
double computeAlpha() const;                          // Tính hệ số alpha
```

#### **LowPassFilter** (Lọc thông thấp)

Cho phép các tần số thấp đi qua, chặn tần số cao.

- Khử high-frequency noise
- Cutoff frequency điển hình: 500 Hz - 5 kHz

#### **HighPassFilter** (Lọc thông cao)

Cho phép các tần số cao đi qua, chặn tần số thấp.

- Loại bỏ DC offset và low-frequency rumble
- Cutoff frequency điển hình: 20 Hz - 200 Hz

#### **BandPassFilter** (Lọc thông dải)

Cho phép một dải tần số cụ thể đi qua.

- Kết hợp Low-Pass và High-Pass
- Ứng dụng: Cách ly tín hiệu trong dải tần số cụ thể

---

### 5. **AudioEffect** (`AudioComponents.h`)

Tạo các hiệu ứng âm thanh.

**Phương thức:**

```cpp
// Echo effect (độ trễ & phản hồi)
void echo(AudioSignal& signal, double delaySeconds, double feedback = 0.4)

// Reverb effect (phòng rộng)
void reverb(AudioSignal& signal, double roomSize = 0.5)
```

**Ví dụ:**

```cpp
AudioEffect effect(0.5, 100.0);  // wetDry=0.5, delayMs=100
effect.echo(signal, 0.05, 0.45);  // Echo 50ms, feedback 45%
```

---

### 6. **AudioFile** (`AudioFile.h`)

Quản lý file âm thanh (WAV format).

**Cấu trúc WAV Header:**

```cpp
struct WAVHeader {
    char     riff[4];               // "RIFF"
    uint32_t chunkSize;             // Kích thước file - 8
    char     wave[4];               // "WAVE"
    char     fmt[4];                // "fmt "
    uint32_t subchunk1Size;         // 16 (cho PCM)
    uint16_t audioFormat;           // 1 (PCM)
    uint16_t numChannels;           // 1=mono, 2=stereo
    uint32_t sampleRate;            // 44100, 48000, etc.
    uint32_t byteRate;              // sampleRate * numChannels * bitDepth/8
    uint16_t blockAlign;            // numChannels * bitDepth/8
    uint16_t bitsPerSample;         // 8, 16, 24, 32
    char     data[4];               // "data"
    uint32_t subchunk2Size;         // Kích thước dữ liệu
};
```

---

## 🎯 Các Tính Năng Chính

### 1. **Tạo Tín Hiệu**

```cpp
// Tạo sóng sin 440 Hz, 0.1 giây, biên độ 0.8
AudioSignal sine = AudioGenerator::generateSine(440.0, 0.1, 0.8);

// Tạo sóng vuông 200 Hz
AudioSignal square = AudioGenerator::generateSquare(200.0, 0.05);

// Tạo noise trắng
AudioSignal noise = AudioGenerator::generateWhiteNoise(0.05);
```

### 2. **Xử Lý Tín Hiệu**

```cpp
AudioProcessor proc;

// Khuyếch đại x2.5
proc.amplify(signal, 2.5);

// Chuẩn hoá peak = 1.0
proc.normalize(signal);

// Trộn 2 tín hiệu (70% tín hiệu thứ 1)
AudioSignal mixed = proc.mix(signal1, signal2, 0.7);

// Fade in 4410 mẫu
proc.fadeIn(signal, 4410);
```

### 3. **Lọc Tín Hiệu**

```cpp
// Low-Pass Filter - Cutoff 500 Hz
LowPassFilter lpf(500.0, 44100.0);
lpf.apply(signal);

// High-Pass Filter - Cutoff 80 Hz
HighPassFilter hpf(80.0, 44100.0);
hpf.apply(signal);

// Band-Pass Filter - 200Hz đến 2kHz
BandPassFilter bpf(200.0, 2000.0, 44100.0);
bpf.apply(signal);
```

### 4. **Hiệu Ứng**

```cpp
AudioEffect effect(0.5, 100.0);

// Echo - Delay 50ms, Feedback 45%
effect.echo(signal, 0.05, 0.45);

// Reverb - Room size 0.6
effect.reverb(signal, 0.6);
```

### 5. **Phân Tích Tín Hiệu**

```cpp
// Biên độ cực đại
double peak = signal.getPeakAmplitude();

// RMS (Root Mean Square)
double rms = signal.getRMS();

// Mức dBFS (dB Full Scale)
double dbfs = signal.getdBFS();

// Thời lượng (s)
double duration = signal.getDuration();

// Số lượng mẫu
size_t numSamples = signal.getNumSamples();
```

---

## 📊 Hình Ảnh Demo

### Output của chương trình chính:

```
  +==============================================+
  |   HE THONG XU LY TIN HIEU AM THANH SO      |
  |        Digital Audio Signal System          |
  |   C++ OOP Demo - Project 1                  |
  +==============================================+

  ============================================================
                  1. TAO TIN HIEU SIN 440 Hz (La/A4)
  ============================================================

  [Sine Wave 440 Hz]
  Ten          : Sine 440Hz
  Sample Rate  : 44100 Hz
  Bit Depth    : 16 bit
  So mau       : 4410
  Thoi luong   : 0.100 s
  Bien do dinh : 0.8000
  RMS          : 0.5657
  Muc (dBFS)   : -4.948 dBFS

  -- Waveform: Sine 440 Hz --
        +|    *         *
         |   * *       * *
         |  *   *     *   *
         | *     *   *     *
        0|*       * *       *
         |         *
         | *     *   *     *
         |  *   *     *   *
         |   * *       * *
        -|    *         *
          +--------------------------------------
            0        t(s)
```

---

## 🔧 Yêu Cầu Biên Dịch

**Compiler:** GCC, Clang, MSVC (C++11 trở lên)

**Thư viện chuẩn:**

- `<iostream>` - Input/Output
- `<vector>` - Dynamic arrays
- `<cmath>` - Toán học
- `<fstream>` - File I/O
- `<algorithm>` - STL algorithms
- `<numeric>` - Numeric algorithms

**Biên dịch:**

```bash
g++ -std=c++11 main.cpp -o audio_system -lm
./audio_system
```

---

## � Hướng Dẫn Chạy Dự Án
// Linh: g++ -std=c++17 main.cpp -Ilib -Llib -lraylib -lopengl32 -lgdi32 -lwinmm -o audio_demo.exe
          ./audio_demo.exe

### **Phương pháp 1: Dùng lệnh g++ trực tiếp**

#### Trên Linux/MacOS:

```bash
cd /path/to/Project1
g++ -std=c++11 main.cpp -o audio_system -lm
./audio_system
```

#### Trên Windows (PowerShell):

```powershell
cd C:\path\to\Project1
g++ -std=c++11 main.cpp -o audio_system.exe -lm
.\audio_system.exe
```

#### Trên Windows (Command Prompt):

```cmd
cd C:\path\to\Project1
g++ -std=c++11 main.cpp -o audio_system.exe -lm
audio_system.exe
```

---

### **Phương pháp 2: Dùng Makefile**

```bash
cd /path/to/Project1
make              # Biên dịch
make run          # Chạy chương trình
make clean        # Xoá file biên dịch
```

---

### **Phương pháp 3: Dùng Compiler MSVC (Windows)**

```bash
cl /std:c++17 main.cpp /link
audio_system.exe
```

---

### **Phương pháp 4: Dùng Clang**

```bash
clang++ -std=c++11 main.cpp -o audio_system -lm
./audio_system
```

---

## 📋 Các Bước Chạy Chi Tiết

### **Bước 1: Kiểm tra phiên bản Compiler**

```bash
# Kiểm tra GCC
g++ --version

# Kiểm tra Clang
clang++ --version

# Kiểm tra MSVC
cl.exe /?
```

### **Bước 2: Điều hướng đến folder dự án**

```bash
cd F:\.vscode\Project1
```

### **Bước 3: Biên dịch mã nguồn**

```bash
g++ -std=c++11 main.cpp -o audio_system -lm
```

**Giải thích các flag:**

- `-std=c++11` - Sử dụng tiêu chuẩn C++11
- `main.cpp` - File nguồn
- `-o audio_system` - Tên file output
- `-lm` - Link thư viện Math (libm)

### **Bước 4: Chạy chương trình**

```bash
./audio_system        # Linux/MacOS
audio_system.exe      # Windows
```

---

## ✅ Output Mong Đợi

Khi chạy thành công, chương trình sẽ hiển thị:

```
  +==============================================+
  |   HE THONG XU LY TIN HIEU AM THANH SO      |
  |        Digital Audio Signal System          |
  |   C++ OOP Demo - Project 1                  |
  +==============================================+

  [Các phần demo: tạo tín hiệu, so sánh sóng, lọc, hiệu ứng, v.v.]
  [Hiển thị waveform, spectrum, thống kê]
```

---

## 🐛 Xử Lý Sự Cố

### **Lỗi: Command 'g++' not found**

**Giải pháp:**

- Linux: `sudo apt-get install build-essential`
- MacOS: Cài XCode Command Line Tools
- Windows: Cài MinGW hoặc MSVC

### **Lỗi: undefined reference to 'sin', 'cos', 'sqrt'**

**Giải pháp:** Thêm flag `-lm` khi biên dịch:

```bash
g++ -std=c++11 main.cpp -o audio_system -lm
```

### **Lỗi: Header files not found**

**Giải pháp:** Đảm bảo tất cả file `.h` trong cùng folder với `main.cpp`

### **Chương trình không chạy trên Windows**

**Giải pháp:**

```powershell
# Chạy với đường dẫn đầy đủ
.\audio_system.exe

# Hoặc
python -m audiowin.exe
```

---

## 🎯 Cách Sử Dụng Chương Trình

Chương trình demo sẽ tự động:

1. **Tạo tín hiệu sin 440 Hz** - Âm La (A4)
2. **So sánh 4 dạng sóng** - Sine, Square, Triangle, Noise
3. **Khuyếch đại & Chuẩn hoá** - Xử lý biên độ
4. **Áp dụng bộ lọc** - Low-Pass, High-Pass, Band-Pass
5. **Thêm hiệu ứng** - Echo, Reverb
6. **Hiển thị thống kê** - Min, Max, Mean, Variance, RMS, dBFS

---

## 📝 Ghi Chú Khi Chạy

- Chương trình chạy hoàn toàn text-based (không cần GUI)
- Tất cả output hiển thị trên terminal/console
- Thời gian chạy: ~2-3 giây (tùy máy tính)
- Không tạo file output (chỉ demo text)
- Có thể redirect output sang file: `./audio_system > output.txt`

---

## �📁 Cấu Trúc File

```
Project1/
├── AudioSignal.h          # Lớp cơ bản - Tín hiệu âm thanh
├── AudioComponents.h      # AudioProcessor, Filters, Effects
├── AudioFile.h            # AudioGenerator, AudioFile, WAV Header
├── main.cpp              # Chương trình demo chính
├── Makefile              # Build script
└── README.md             # Tài liệu này
```

---

## 🎓 Các Khái Niệm DSP Được Sử Dụng

### 1. **Sample Rate (Tần số lấy mẫu)**

- Số lần lấy mẫu tín hiệu analog mỗi giây
- 44100 Hz = CD quality
- 48000 Hz = Professional audio/video
- Nyquist Theorem: Sample rate ≥ 2 × Max frequency

### 2. **Bit Depth (Độ sâu bit)**

- Số bit dùng để biểu diễn mỗi mẫu
- 16-bit = ±32,768 (CD quality)
- 24-bit = ±8,388,608 (Professional)

### 3. **RMS (Root Mean Square)**

- Đo lường năng lượng của tín hiệu
- RMS = √(Σ(sample²) / N)
- Biểu thị "loudness" của tín hiệu

### 4. **dBFS (dB Full Scale)**

- Đơn vị đo lường mức âm thanh
- dBFS = 20 × log₁₀(RMS)
- Chuẩn hóa so với full scale (1.0)

### 5. **Digital Filters**

- **Low-Pass**: Cho tần số thấp đi qua
- **High-Pass**: Cho tần số cao đi qua
- **Band-Pass**: Cho dải tần số cụ thể đi qua
- Sử dụng: Khử nhiễu, tách tín hiệu

### 6. **Audio Effects**

- **Echo**: Tín hiệu được lặp lại sau độ trễ
- **Reverb**: Mô phỏng phòng nghe, tiếng vang tự nhiên

---

## 💡 Ứng Dụng Thực Tế

✅ **Xử lý âm thanh trong DAW** (Digital Audio Workstation)
✅ **Chỉnh sửa âm thanh** (normalize, amplify, effects)
✅ **Khử nhiễu từ ghi âm** (filters)
✅ **Phân tích phổ âm thanh** (spectrum analysis)
✅ **Tạo nhạc tổng hợp** (synthesizer)
✅ **Nén/Giải nén âm thanh** (codecs)

---

## 🚀 Hướng Mở Rộng Trong Tương Lai

- 🎚️ Thêm Equalizer (EQ) 10-band
- 🎙️ Thêm Compression và Limiting effects
- 📊 GUI visualization (SDL, SFML, Qt)
- 💾 Hỗ trợ đọc/ghi file MP3, FLAC
- 🎵 Real-time audio processing
- 🎛️ Plugin system cho custom effects

---

## 📝 Ghi Chú

- Tất cả mẫu âm thanh được chuẩn hoá từ -1.0 đến 1.0
- Không hỗ trợ multi-channel processing (chỉ mono/stereo cơ bản)
- Filters sử dụng simple first-order implementation
- Hiệu ứng là fake/simplified models

---

## 👨‍💻 Tác Giả

**Project:** Digital Audio Signal Processing System  
**Ngôn ngữ:** C++  
**Năm:** 2026  
**Mục đích:** Educational - Học OOP & DSP concepts

---

## 📚 Tham Khảo

- Digital Signal Processing Theory
- C++ Object-Oriented Programming
- Audio Engineering Fundamentals
- WAV File Format Specification
