#ifndef VN_LCD_H
#define VN_LCD_H

#include <Arduino.h>
#include <Print.h>
#include <Wire.h>

class VN_LCD : public Print {
public:
    VN_LCD(uint8_t address = 0x27, uint8_t columns = 16, uint8_t rows = 2);

    bool begin();
    bool begin(int sdaPin, int sclPin);
    bool begin(uint8_t address, uint8_t columns, uint8_t rows);
    bool begin(int sdaPin, int sclPin, uint8_t address, uint8_t columns, uint8_t rows);
    bool beginWithScan(Stream& output);
    bool beginWithScan(int sdaPin, int sclPin, Stream& output);

    void setI2CPins(int sdaPin, int sclPin);
    void setAddress(uint8_t address);
    void setGeometry(uint8_t columns, uint8_t rows);
    uint8_t scanI2C(uint8_t addresses[], uint8_t maxAddresses, uint8_t startAddress = 0x08, uint8_t endAddress = 0x77);
    uint8_t scanI2C(Stream& output, uint8_t startAddress = 0x08, uint8_t endAddress = 0x77);
    uint8_t findFirstI2CAddress(uint8_t startAddress = 0x08, uint8_t endAddress = 0x77);

    void clear();
    void clear(uint8_t column, uint8_t row);
    void clear(uint8_t column, uint8_t row, uint8_t length);
    void home();
    void setCursor(uint8_t column, uint8_t row);

    void noDisplay();
    void display();
    void noCursor();
    void cursor();
    void noBlink();
    void blink();
    void scrollDisplayLeft();
    void scrollDisplayRight();
    void leftToRight();
    void rightToLeft();
    void autoscroll();
    void noAutoscroll();

    void setBacklight(bool enabled);
    void backlight();
    void noBacklight();

    void createChar(uint8_t location, const uint8_t charmap[]);
    void clearVietnameseCache();

    void printVietnamese(const char* text);
    void printVietnamese(const String& text);
    void printAt(uint8_t column, uint8_t row, const char* text);
    void printAt(uint8_t column, uint8_t row, const String& text);

    using Print::write;
    size_t write(uint8_t value) override;
    size_t write(const uint8_t* buffer, size_t size) override;

private:
    enum Tone : uint8_t {
        TONE_NONE,
        TONE_ACUTE,
        TONE_GRAVE,
        TONE_HOOK,
        TONE_TILDE,
        TONE_DOT
    };

    enum LetterForm : uint8_t {
        FORM_PLAIN,
        FORM_BREVE,
        FORM_CIRCUMFLEX,
        FORM_HORN,
        FORM_STROKE
    };

    struct GlyphSpec {
        char base;
        uint8_t form;
        uint8_t tone;
    };

    struct GlyphSlot {
        uint32_t codepoint;
        bool used;
        uint8_t pattern[8];
    };

    bool initializeLcd();
    void beginWire();
    bool pingAddress();

    bool i2cProbe(uint8_t address);
    bool i2cWriteByte(uint8_t address, uint8_t value);
    void softI2CBegin();
    void softI2CStart();
    void softI2CStop();
    bool softI2CWrite(uint8_t value);
    void softSetSda(bool high);
    void softSetScl(bool high);
    void softDelay();

    void command(uint8_t value);
    void send(uint8_t value, uint8_t mode);
    void write4bits(uint8_t value);
    void pulseEnable(uint8_t value);
    void expanderWrite(uint8_t value);

    void writeRaw(uint8_t value);
    void advanceCursor();
    void newline();
    void resetUtf8();
    void writeCodepoint(uint32_t codepoint);
    void writeDegreeUnit(char unit);

    bool decodeVietnamese(uint32_t codepoint, GlyphSpec& spec) const;
    uint8_t acquireGlyph(uint32_t codepoint, const GlyphSpec& spec, bool& ok);
    void buildGlyph(const GlyphSpec& spec, uint8_t pattern[8]) const;
    const uint8_t* basePattern(char base, bool compact) const;
    void loadCustomChar(uint8_t location, const uint8_t charmap[]);
    static void setGlyph(GlyphSpec& spec, char base, uint8_t form, uint8_t tone);
    static uint8_t compactTone(uint8_t tone);

    uint8_t _address;
    uint8_t _columns;
    uint8_t _rows;
    uint8_t _displayFunction;
    uint8_t _displayControl;
    uint8_t _displayMode;
    uint8_t _backlightMask;

    int _sdaPin;
    int _sclPin;
    bool _useSoftI2C;
    bool _begun;

    uint8_t _cursorColumn;
    uint8_t _cursorRow;

    uint32_t _utf8Codepoint;
    uint8_t _utf8Remaining;

    GlyphSlot _glyphSlots[8];
};

#endif
