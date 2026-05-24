CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGET   = audio_demo
SRCS     = main.cpp
HEADERS  = AudioSignal.h AudioComponents.h AudioFile.h
# Thư mục chứa raylib.h và libraylib.a
INCLUDES = -Ilib
LIBS     = -Llib -lraylib -lopengl32 -lgdi32 -lwinmm
.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) $(LIBS)
	@echo "Build thanh cong: ./$(TARGET)"

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.wav
	@echo "Da don sach."
