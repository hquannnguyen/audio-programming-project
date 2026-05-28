#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

using namespace std;

class AudioSignal
{
private:
    vector<double> samples;
    int sampleRate;
    int numChannels;
    int bitDepth;
    string name;

public:
    AudioSignal(int sr = 44100, int ch = 1, int bd = 16)
        : sampleRate(sr), numChannels(ch), bitDepth(bd), name("Untitled") {}

    AudioSignal(const vector<double> &s, int sr = 44100, int ch = 1, int bd = 16)
        : samples(s), sampleRate(sr), numChannels(ch), bitDepth(bd), name("Signal") {}

    // FIX 1: Thêm Getter dạng non-const reference để Backend/GUI sửa đổi trực tiếp dữ liệu mảng mẫu
    // không cần qua bước trung gian gán lại setSamples (tránh copy mảng gây chậm luồng thời gian thực)
    vector<double> &getSamplesRef() { return samples; }
    const vector<double> &getSamples() const { return samples; }

    int getSampleRate() const { return sampleRate; }
    int getNumChannels() const { return numChannels; }
    int getBitDepth() const { return bitDepth; }
    const string &getName() const { return name; }

    // FIX 2: Chuẩn hóa lại thời lượng tín hiệu. Đối với việc xử lý tệp hay bộ đệm âm thanh số,
    // thời lượng thực tế của luồng dòng chảy thời gian dựa trên số khung mẫu (Frames).
    double getDuration() const
    {
        if (sampleRate == 0 || numChannels == 0)
            return 0.0;
        return static_cast<double>(samples.size()) / (sampleRate * numChannels);
    }

    size_t getNumSamples() const { return samples.size(); }

    void setSamples(const vector<double> &s) { samples = s; }
    void setName(const string &n) { name = n; }
    void addSample(double s) { samples.push_back(s); }

    // FIX 3: Tối ưu hàm clear, giải phóng bộ nhớ thực tế của vector thay vì chỉ xóa kích thước ẩn
    void clear()
    {
        samples.clear();
        samples.shrink_to_fit();
    }

    double getPeakAmplitude() const
    {
        if (samples.empty())
            return 0.0;
        double peak = 0.0;
        for (double s : samples)
        {
            double absS = abs(s);
            if (absS > peak)
                peak = absS;
        }
        return peak;
    }

    double getRMS() const
    {
        if (samples.empty())
            return 0.0;
        double sum = 0.0;
        for (double s : samples)
            sum += s * s;
        return sqrt(sum / samples.size());
    }

    double getdBFS() const
    {
        double rms = getRMS();
        if (rms <= 0.00001)
            return -96.0; // Tránh lỗi log10(0) gây crash toán học (NaN)
        return 20.0 * log10(rms);
    }

    // FIX 4: Thêm hàm bổ trợ DSP cắt/co dãn kích thước mảng nhanh phục vụ cho khối cửa sổ DFT 4096
    void resizeAndZeroPad(size_t targetSize)
    {
        size_t currentSize = samples.size();
        if (currentSize > targetSize)
        {
            samples.resize(targetSize);
        }
        else if (currentSize < targetSize)
        {
            // Điền các giá trị 0 vào phần thiếu (Zero-padding) - Kỹ thuật kinh điển giúp tăng độ phân giải phổ
            samples.insert(samples.end(), targetSize - currentSize, 0.0);
        }
    }

    void printInfo() const;
};


inline void AudioSignal::printInfo() const
{
    cout << "AudioSignal[" << name << "] "
         << sampleRate << "Hz " << numChannels << "ch "
         << bitDepth << "bit " << getDuration() << "s (Total Samples: " << getNumSamples() << ")\n";
}