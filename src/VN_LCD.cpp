#include "VN_LCD.h"

namespace {
const uint8_t LCD_CLEARDISPLAY = 0x01;
const uint8_t LCD_RETURNHOME = 0x02;
const uint8_t LCD_ENTRYMODESET = 0x04;
const uint8_t LCD_DISPLAYCONTROL = 0x08;
const uint8_t LCD_CURSORSHIFT = 0x10;
const uint8_t LCD_FUNCTIONSET = 0x20;
const uint8_t LCD_SETCGRAMADDR = 0x40;
const uint8_t LCD_SETDDRAMADDR = 0x80;

const uint8_t LCD_ENTRYRIGHT = 0x00;
const uint8_t LCD_ENTRYLEFT = 0x02;
const uint8_t LCD_ENTRYSHIFTINCREMENT = 0x01;
const uint8_t LCD_ENTRYSHIFTDECREMENT = 0x00;

const uint8_t LCD_DISPLAYON = 0x04;
const uint8_t LCD_DISPLAYOFF = 0x00;
const uint8_t LCD_CURSORON = 0x02;
const uint8_t LCD_CURSOROFF = 0x00;
const uint8_t LCD_BLINKON = 0x01;
const uint8_t LCD_BLINKOFF = 0x00;

const uint8_t LCD_DISPLAYMOVE = 0x08;
const uint8_t LCD_CURSORMOVE = 0x00;
const uint8_t LCD_MOVERIGHT = 0x04;
const uint8_t LCD_MOVELEFT = 0x00;

const uint8_t LCD_8BITMODE = 0x10;
const uint8_t LCD_4BITMODE = 0x00;
const uint8_t LCD_2LINE = 0x08;
const uint8_t LCD_1LINE = 0x00;
const uint8_t LCD_5x10DOTS = 0x04;
const uint8_t LCD_5x8DOTS = 0x00;

const uint8_t LCD_BACKLIGHT = 0x08;
const uint8_t LCD_NOBACKLIGHT = 0x00;

const uint8_t PIN_ENABLE = 0x04;
const uint8_t PIN_READ_WRITE = 0x02;
const uint8_t PIN_REGISTER_SELECT = 0x01;

}

VN_LCD::VN_LCD(uint8_t address, uint8_t columns, uint8_t rows)
    : _address(address),
      _columns(columns),
      _rows(rows),
      _displayFunction(LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS),
      _displayControl(0),
      _displayMode(0),
      _backlightMask(LCD_BACKLIGHT),
      _sdaPin(-1),
      _sclPin(-1),
      _useSoftI2C(false),
      _begun(false),
      _cursorColumn(0),
      _cursorRow(0),
      _utf8Codepoint(0),
      _utf8Remaining(0) {
    clearVietnameseCache();
}

void VN_LCD::setGlyph(GlyphSpec& spec, char base, uint8_t form, uint8_t tone) {
    spec.base = base;
    spec.form = form;
    spec.tone = tone;
}

uint8_t VN_LCD::compactTone(uint8_t tone) {
    switch (tone) {
        case TONE_ACUTE: return 0x02;
        case TONE_GRAVE: return 0x08;
        case TONE_HOOK: return 0x06;
        case TONE_TILDE: return 0x0A;
        default: return 0x00;
    }
}

bool VN_LCD::begin() {
    beginWire();
    if (!pingAddress()) {
        _begun = false;
        return false;
    }

    _begun = initializeLcd();
    return _begun;
}

bool VN_LCD::begin(int sdaPin, int sclPin) {
    setI2CPins(sdaPin, sclPin);
    return begin();
}

bool VN_LCD::begin(uint8_t address, uint8_t columns, uint8_t rows) {
    setAddress(address);
    setGeometry(columns, rows);
    return begin();
}

bool VN_LCD::begin(int sdaPin, int sclPin, uint8_t address, uint8_t columns, uint8_t rows) {
    setI2CPins(sdaPin, sclPin);
    setAddress(address);
    setGeometry(columns, rows);
    return begin();
}
bool VN_LCD::beginWithScan(int sdaPin, int sclPin, Stream& output) {
    setI2CPins(sdaPin, sclPin);
    return beginWithScan(output);
}

bool VN_LCD::beginWithScan(Stream& output) {
    uint8_t requestedAddress = _address;
    if (begin()) {
        return true;
    }

    output.print(F("Khong tim thay LCD tai dia chi 0x"));
    if (requestedAddress < 16) {
        output.print('0');
    }
    output.print(requestedAddress, HEX);
    output.println(F(". Dang scan I2C..."));

    uint8_t addresses[16];
    uint8_t count = scanI2C(addresses, sizeof(addresses));
    if (count == 0) {
        output.println(F("Khong scan duoc thiet bi I2C nao. Kiem tra SDA, SCL, VCC, GND va dien tro pull-up."));
        return false;
    }

    output.println(F("Cac dia chi I2C tim thay:"));
    for (uint8_t i = 0; i < count; i++) {
        output.print(F(" - 0x"));
        if (addresses[i] < 16) {
            output.print('0');
        }
        output.println(addresses[i], HEX);
    }

    const uint8_t lcdCandidates[] = {
        0x27, 0x3F,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E
    };

    for (uint8_t c = 0; c < sizeof(lcdCandidates); c++) {
        uint8_t candidate = lcdCandidates[c];
        if (candidate == requestedAddress) {
            continue;
        }

        bool found = false;
        for (uint8_t i = 0; i < count; i++) {
            if (addresses[i] == candidate) {
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }

        setAddress(candidate);
        output.print(F("Thu LCD tai dia chi 0x"));
        if (candidate < 16) {
            output.print('0');
        }
        output.println(candidate, HEX);

        if (begin()) {
            output.print(F("Da ket noi LCD. Nen sua dia chi trong code thanh 0x"));
            if (candidate < 16) {
                output.print('0');
            }
            output.println(candidate, HEX);
            return true;
        }
    }

    if (count == 1 && addresses[0] != requestedAddress) {
        setAddress(addresses[0]);
        output.print(F("Thu thiet bi I2C duy nhat tai dia chi 0x"));
        if (addresses[0] < 16) {
            output.print('0');
        }
        output.println(addresses[0], HEX);

        if (begin()) {
            output.print(F("Da ket noi LCD. Nen sua dia chi trong code thanh 0x"));
            if (addresses[0] < 16) {
                output.print('0');
            }
            output.println(addresses[0], HEX);
            return true;
        }
    }

    setAddress(requestedAddress);
    _begun = false;
    output.println(F("Co thiet bi I2C nhung khong khoi tao duoc LCD. Hay kiem tra module LCD/backpack."));
    return false;
}

void VN_LCD::setI2CPins(int sdaPin, int sclPin) {
    _sdaPin = sdaPin;
    _sclPin = sclPin;
}

void VN_LCD::setAddress(uint8_t address) {
    _address = address;
}

void VN_LCD::setGeometry(uint8_t columns, uint8_t rows) {
    _columns = columns;
    _rows = rows;
}

uint8_t VN_LCD::scanI2C(uint8_t addresses[], uint8_t maxAddresses, uint8_t startAddress, uint8_t endAddress) {
    beginWire();

    if (addresses == nullptr || maxAddresses == 0) {
        return 0;
    }
    if (startAddress > endAddress) {
        uint8_t temp = startAddress;
        startAddress = endAddress;
        endAddress = temp;
    }
    if (endAddress > 0x7F) {
        endAddress = 0x7F;
    }

    uint8_t count = 0;
    for (uint8_t address = startAddress; address <= endAddress; address++) {
        if (i2cProbe(address)) {
            addresses[count++] = address;
            if (count >= maxAddresses) {
                break;
            }
        }
        if (address == 0x7F) {
            break;
        }
    }
    return count;
}

uint8_t VN_LCD::scanI2C(Stream& output, uint8_t startAddress, uint8_t endAddress) {
    uint8_t addresses[16];
    uint8_t count = scanI2C(addresses, sizeof(addresses), startAddress, endAddress);

    if (count == 0) {
        output.println(F("Khong tim thay thiet bi I2C nao."));
        return 0;
    }

    output.print(F("Tim thay "));
    output.print(count);
    output.println(F(" thiet bi I2C:"));
    for (uint8_t i = 0; i < count; i++) {
        output.print(F(" - 0x"));
        if (addresses[i] < 16) {
            output.print('0');
        }
        output.println(addresses[i], HEX);
    }
    return count;
}

uint8_t VN_LCD::findFirstI2CAddress(uint8_t startAddress, uint8_t endAddress) {
    uint8_t address = 0;
    return scanI2C(&address, 1, startAddress, endAddress) == 1 ? address : 0;
}
void VN_LCD::beginWire() {
    if (_sdaPin >= 0 && _sclPin >= 0) {
        _useSoftI2C = true;
        softI2CBegin();
        return;
    }

    _useSoftI2C = false;
    Wire.begin();
}

bool VN_LCD::i2cProbe(uint8_t address) {
    if (_useSoftI2C) {
        softI2CStart();
        bool ack = softI2CWrite(static_cast<uint8_t>(address << 1));
        softI2CStop();
        return ack;
    }

    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool VN_LCD::i2cWriteByte(uint8_t address, uint8_t value) {
    if (_useSoftI2C) {
        softI2CStart();
        bool addressAck = softI2CWrite(static_cast<uint8_t>(address << 1));
        bool dataAck = softI2CWrite(value);
        softI2CStop();
        return addressAck && dataAck;
    }

    Wire.beginTransmission(address);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

void VN_LCD::softI2CBegin() {
    digitalWrite(_sdaPin, LOW);
    digitalWrite(_sclPin, LOW);
    softSetSda(true);
    softSetScl(true);
}

void VN_LCD::softI2CStart() {
    softSetSda(true);
    softSetScl(true);
    softDelay();
    softSetSda(false);
    softDelay();
    softSetScl(false);
}

void VN_LCD::softI2CStop() {
    softSetSda(false);
    softDelay();
    softSetScl(true);
    softDelay();
    softSetSda(true);
    softDelay();
}

bool VN_LCD::softI2CWrite(uint8_t value) {
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        softSetSda((value & mask) != 0);
        softDelay();
        softSetScl(true);
        softDelay();
        softSetScl(false);
    }

    softSetSda(true);
    softDelay();
    softSetScl(true);
    softDelay();
    bool ack = digitalRead(_sdaPin) == LOW;
    softSetScl(false);
    return ack;
}

void VN_LCD::softSetSda(bool high) {
    if (high) {
        pinMode(_sdaPin, INPUT_PULLUP);
    } else {
        digitalWrite(_sdaPin, LOW);
        pinMode(_sdaPin, OUTPUT);
    }
}

void VN_LCD::softSetScl(bool high) {
    if (high) {
        pinMode(_sclPin, INPUT_PULLUP);
    } else {
        digitalWrite(_sclPin, LOW);
        pinMode(_sclPin, OUTPUT);
    }
}

void VN_LCD::softDelay() {
    delayMicroseconds(5);
}
bool VN_LCD::pingAddress() {
    return i2cProbe(_address);
}

bool VN_LCD::initializeLcd() {
    _displayFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    if (_rows > 1) {
        _displayFunction |= LCD_2LINE;
    }

    delay(50);
    expanderWrite(0x00);
    delay(50);

    write4bits(0x30);
    delayMicroseconds(4500);
    write4bits(0x30);
    delayMicroseconds(4500);
    write4bits(0x30);
    delayMicroseconds(150);
    write4bits(0x20);

    command(LCD_FUNCTIONSET | _displayFunction);

    _displayControl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    display();

    clear();

    _displayMode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    command(LCD_ENTRYMODESET | _displayMode);
    home();
    return true;
}

void VN_LCD::clear() {
    command(LCD_CLEARDISPLAY);
    delayMicroseconds(2000);
    _cursorColumn = 0;
    _cursorRow = 0;
    resetUtf8();
    clearVietnameseCache();
}

void VN_LCD::clear(uint8_t column, uint8_t row) {
    clear(column, row, 1);
}

void VN_LCD::clear(uint8_t column, uint8_t row, uint8_t length) {
    if (!_begun || length == 0 || row >= _rows || column >= _columns) {
        return;
    }

    uint8_t available = _columns - column;
    uint8_t cells = length < available ? length : available;

    resetUtf8();
    setCursor(column, row);
    for (uint8_t i = 0; i < cells; i++) {
        writeRaw(' ');
    }
    setCursor(column, row);
}

void VN_LCD::home() {
    command(LCD_RETURNHOME);
    delayMicroseconds(2000);
    _cursorColumn = 0;
    _cursorRow = 0;
    resetUtf8();
}

void VN_LCD::setCursor(uint8_t column, uint8_t row) {
    static const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
    if (_rows == 0 || _columns == 0) {
        return;
    }
    if (row >= _rows) {
        row = _rows - 1;
    }
    if (row > 3) {
        row = 3;
    }
    if (column >= _columns) {
        column = _columns - 1;
    }
    command(LCD_SETDDRAMADDR | (column + rowOffsets[row]));
    _cursorColumn = column;
    _cursorRow = row;
    resetUtf8();
}

void VN_LCD::noDisplay() {
    _displayControl &= ~LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void VN_LCD::display() {
    _displayControl |= LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void VN_LCD::noCursor() {
    _displayControl &= ~LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void VN_LCD::cursor() {
    _displayControl |= LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void VN_LCD::noBlink() {
    _displayControl &= ~LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void VN_LCD::blink() {
    _displayControl |= LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void VN_LCD::scrollDisplayLeft() {
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void VN_LCD::scrollDisplayRight() {
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void VN_LCD::leftToRight() {
    _displayMode |= LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _displayMode);
}

void VN_LCD::rightToLeft() {
    _displayMode &= ~LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _displayMode);
}

void VN_LCD::autoscroll() {
    _displayMode |= LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _displayMode);
}

void VN_LCD::noAutoscroll() {
    _displayMode &= ~LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _displayMode);
}

void VN_LCD::setBacklight(bool enabled) {
    _backlightMask = enabled ? LCD_BACKLIGHT : LCD_NOBACKLIGHT;
    if (_begun) {
        expanderWrite(0x00);
    }
}

void VN_LCD::backlight() {
    setBacklight(true);
}

void VN_LCD::noBacklight() {
    setBacklight(false);
}

void VN_LCD::createChar(uint8_t location, const uint8_t charmap[]) {
    location &= 0x07;
    loadCustomChar(location, charmap);
    _glyphSlots[location].used = true;
    _glyphSlots[location].codepoint = 0xFFFFFF00UL | location;
    for (uint8_t i = 0; i < 8; i++) {
        _glyphSlots[location].pattern[i] = charmap[i];
    }
}

void VN_LCD::clearVietnameseCache() {
    for (uint8_t i = 0; i < 8; i++) {
        _glyphSlots[i].codepoint = 0;
        _glyphSlots[i].used = false;
        for (uint8_t row = 0; row < 8; row++) {
            _glyphSlots[i].pattern[row] = 0;
        }
    }
}

void VN_LCD::printVietnamese(const char* text) {
    if (text == nullptr) {
        return;
    }
    print(text);
}

void VN_LCD::printVietnamese(const String& text) {
    print(text);
}

void VN_LCD::printAt(uint8_t column, uint8_t row, const char* text) {
    setCursor(column, row);
    printVietnamese(text);
}

void VN_LCD::printAt(uint8_t column, uint8_t row, const String& text) {
    setCursor(column, row);
    printVietnamese(text);
}

size_t VN_LCD::write(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (buffer[i] == '0' && (i + 1) < size &&
            (buffer[i + 1] == 'C' || buffer[i + 1] == 'c' ||
             buffer[i + 1] == 'F' || buffer[i + 1] == 'f')) {
            writeDegreeUnit(static_cast<char>(buffer[i + 1]));
            i++;
        } else {
            write(buffer[i]);
        }
    }
    return size;
}

size_t VN_LCD::write(uint8_t value) {
    if (!_begun) {
        return 1;
    }

    if (_utf8Remaining > 0) {
        if ((value & 0xC0) == 0x80) {
            _utf8Codepoint = (_utf8Codepoint << 6) | (value & 0x3F);
            _utf8Remaining--;
            if (_utf8Remaining == 0) {
                writeCodepoint(_utf8Codepoint);
                _utf8Codepoint = 0;
            }
            return 1;
        }

        resetUtf8();
        writeRaw('?');
        if (value >= 0x80) {
            writeRaw('?');
            return 1;
        }
    }

    if (value < 0x80) {
        if (value < 8) {
            writeRaw(value);
        } else if (value == '\n') {
            newline();
        } else if (value == '\r') {
            return 1;
        } else if (value == '\t') {
            writeRaw(' ');
            writeRaw(' ');
        } else if (value >= 0x20) {
            if (value >= 'A' && value <= 'Z') {
                value = value + ('a' - 'A');
            }
            writeRaw(value);
        }
        return 1;
    }

    if (value >= 0xC2 && value <= 0xDF) {
        _utf8Codepoint = value & 0x1F;
        _utf8Remaining = 1;
    } else if (value >= 0xE0 && value <= 0xEF) {
        _utf8Codepoint = value & 0x0F;
        _utf8Remaining = 2;
    } else if (value >= 0xF0 && value <= 0xF4) {
        _utf8Codepoint = value & 0x07;
        _utf8Remaining = 3;
    } else {
        writeRaw('?');
    }

    return 1;
}

void VN_LCD::command(uint8_t value) {
    send(value, 0);
}

void VN_LCD::send(uint8_t value, uint8_t mode) {
    uint8_t highNibble = value & 0xF0;
    uint8_t lowNibble = (value << 4) & 0xF0;
    write4bits(highNibble | mode);
    write4bits(lowNibble | mode);
}

void VN_LCD::write4bits(uint8_t value) {
    expanderWrite(value);
    pulseEnable(value);
}

void VN_LCD::pulseEnable(uint8_t value) {
    expanderWrite(value | PIN_ENABLE);
    delayMicroseconds(1);
    expanderWrite(value & ~PIN_ENABLE);
    delayMicroseconds(50);
}

void VN_LCD::expanderWrite(uint8_t value) {
    i2cWriteByte(_address, static_cast<uint8_t>((value & ~PIN_READ_WRITE) | _backlightMask));
}

void VN_LCD::writeRaw(uint8_t value) {
    send(value, PIN_REGISTER_SELECT);
    advanceCursor();
}

void VN_LCD::advanceCursor() {
    if (_columns == 0 || _rows == 0) {
        return;
    }

    _cursorColumn++;
    if (_cursorColumn >= _columns) {
        _cursorColumn = 0;
        _cursorRow = (_cursorRow + 1) % _rows;
        setCursor(_cursorColumn, _cursorRow);
    }
}

void VN_LCD::newline() {
    if (_rows == 0) {
        return;
    }
    _cursorColumn = 0;
    _cursorRow = (_cursorRow + 1) % _rows;
    setCursor(_cursorColumn, _cursorRow);
}

void VN_LCD::resetUtf8() {
    _utf8Codepoint = 0;
    _utf8Remaining = 0;
}

void VN_LCD::writeDegreeUnit(char unit) {
    static const uint8_t degreeC[8] = {
        0x10,
        0x07,
        0x08,
        0x08,
        0x08,
        0x08,
        0x07,
        0x00
    };
    static const uint8_t degreeF[8] = {
        0x00,
        0x17,
        0x04,
        0x07,
        0x04,
        0x04,
        0x04,
        0x00
    };

    bool isF = unit == 'F' || unit == 'f';
    const uint8_t* pattern = isF ? degreeF : degreeC;
    uint32_t key = isF ? 0x564E3046UL : 0x564E3043UL;

    resetUtf8();
    for (uint8_t i = 0; i < 8; i++) {
        if (_glyphSlots[i].used && _glyphSlots[i].codepoint == key) {
            writeRaw(i);
            return;
        }
    }

    for (uint8_t i = 0; i < 8; i++) {
        if (!_glyphSlots[i].used) {
            loadCustomChar(i, pattern);
            _glyphSlots[i].codepoint = key;
            _glyphSlots[i].used = true;
            for (uint8_t row = 0; row < 8; row++) {
                _glyphSlots[i].pattern[row] = pattern[row];
            }
            writeRaw(i);
            return;
        }
    }

    writeRaw('0');
    writeRaw(isF ? 'F' : 'C');
}
void VN_LCD::writeCodepoint(uint32_t codepoint) {
    if (codepoint < 0x80) {
        write(static_cast<uint8_t>(codepoint));
        return;
    }

    if (codepoint >= 0x0300 && codepoint <= 0x036F) {
        return;
    }

    GlyphSpec spec;
    if (!decodeVietnamese(codepoint, spec)) {
        writeRaw('?');
        return;
    }

    if (spec.base >= 'a' && spec.base <= 'z') {
        spec.base = spec.base - ('a' - 'A');
    }

    bool stackedVowelMark = spec.form == FORM_BREVE ||
                            spec.form == FORM_CIRCUMFLEX ||
                            spec.form == FORM_HORN;
    if (stackedVowelMark && spec.tone != TONE_DOT) {
        spec.tone = TONE_NONE;
    }

    uint32_t glyphKey = 0x564E0000UL |
                        (static_cast<uint32_t>(static_cast<uint8_t>(spec.base)) << 8) |
                        (static_cast<uint32_t>(spec.form) << 4) |
                        static_cast<uint32_t>(spec.tone);

    bool ok = false;
    uint8_t glyph = acquireGlyph(glyphKey, spec, ok);
    if (ok) {
        writeRaw(glyph);
        return;
    }

    uint8_t fallback = static_cast<uint8_t>(spec.base);
    if (fallback >= 'A' && fallback <= 'Z') {
        fallback = fallback + ('a' - 'A');
    }
    writeRaw(fallback);
}

bool VN_LCD::decodeVietnamese(uint32_t codepoint, GlyphSpec& spec) const {
    setGlyph(spec, '?', FORM_PLAIN, TONE_NONE);
    static const uint8_t toneOrder[] = {TONE_ACUTE, TONE_GRAVE, TONE_HOOK, TONE_TILDE, TONE_DOT};

    uint8_t offset = 0;
    if (codepoint >= 0x1EA4 && codepoint <= 0x1EAD) {
        offset = codepoint - 0x1EA4;
        setGlyph(spec, (offset & 1) ? 'a' : 'A', FORM_CIRCUMFLEX, toneOrder[offset / 2]);
        return true;
    }
    if (codepoint >= 0x1EAE && codepoint <= 0x1EB7) {
        offset = codepoint - 0x1EAE;
        setGlyph(spec, (offset & 1) ? 'a' : 'A', FORM_BREVE, toneOrder[offset / 2]);
        return true;
    }
    if (codepoint >= 0x1EBE && codepoint <= 0x1EC7) {
        offset = codepoint - 0x1EBE;
        setGlyph(spec, (offset & 1) ? 'e' : 'E', FORM_CIRCUMFLEX, toneOrder[offset / 2]);
        return true;
    }
    if (codepoint >= 0x1ED0 && codepoint <= 0x1ED9) {
        offset = codepoint - 0x1ED0;
        setGlyph(spec, (offset & 1) ? 'o' : 'O', FORM_CIRCUMFLEX, toneOrder[offset / 2]);
        return true;
    }
    if (codepoint >= 0x1EDA && codepoint <= 0x1EE3) {
        offset = codepoint - 0x1EDA;
        setGlyph(spec, (offset & 1) ? 'o' : 'O', FORM_HORN, toneOrder[offset / 2]);
        return true;
    }
    if (codepoint >= 0x1EE8 && codepoint <= 0x1EF1) {
        offset = codepoint - 0x1EE8;
        setGlyph(spec, (offset & 1) ? 'u' : 'U', FORM_HORN, toneOrder[offset / 2]);
        return true;
    }

    switch (codepoint) {
        case 0x00C0: setGlyph(spec, 'A', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00C1: setGlyph(spec, 'A', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00C2: setGlyph(spec, 'A', FORM_CIRCUMFLEX, TONE_NONE); return true;
        case 0x00C3: setGlyph(spec, 'A', FORM_PLAIN, TONE_TILDE); return true;
        case 0x00C8: setGlyph(spec, 'E', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00C9: setGlyph(spec, 'E', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00CA: setGlyph(spec, 'E', FORM_CIRCUMFLEX, TONE_NONE); return true;
        case 0x00CC: setGlyph(spec, 'I', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00CD: setGlyph(spec, 'I', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00D2: setGlyph(spec, 'O', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00D3: setGlyph(spec, 'O', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00D4: setGlyph(spec, 'O', FORM_CIRCUMFLEX, TONE_NONE); return true;
        case 0x00D5: setGlyph(spec, 'O', FORM_PLAIN, TONE_TILDE); return true;
        case 0x00D9: setGlyph(spec, 'U', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00DA: setGlyph(spec, 'U', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00DD: setGlyph(spec, 'Y', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00E0: setGlyph(spec, 'a', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00E1: setGlyph(spec, 'a', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00E2: setGlyph(spec, 'a', FORM_CIRCUMFLEX, TONE_NONE); return true;
        case 0x00E3: setGlyph(spec, 'a', FORM_PLAIN, TONE_TILDE); return true;
        case 0x00E8: setGlyph(spec, 'e', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00E9: setGlyph(spec, 'e', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00EA: setGlyph(spec, 'e', FORM_CIRCUMFLEX, TONE_NONE); return true;
        case 0x00EC: setGlyph(spec, 'i', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00ED: setGlyph(spec, 'i', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00F2: setGlyph(spec, 'o', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00F3: setGlyph(spec, 'o', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00F4: setGlyph(spec, 'o', FORM_CIRCUMFLEX, TONE_NONE); return true;
        case 0x00F5: setGlyph(spec, 'o', FORM_PLAIN, TONE_TILDE); return true;
        case 0x00F9: setGlyph(spec, 'u', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x00FA: setGlyph(spec, 'u', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x00FD: setGlyph(spec, 'y', FORM_PLAIN, TONE_ACUTE); return true;
        case 0x0102: setGlyph(spec, 'A', FORM_BREVE, TONE_NONE); return true;
        case 0x0103: setGlyph(spec, 'a', FORM_BREVE, TONE_NONE); return true;
        case 0x0110: setGlyph(spec, 'D', FORM_STROKE, TONE_NONE); return true;
        case 0x0111: setGlyph(spec, 'd', FORM_STROKE, TONE_NONE); return true;
        case 0x0128: setGlyph(spec, 'I', FORM_PLAIN, TONE_TILDE); return true;
        case 0x0129: setGlyph(spec, 'i', FORM_PLAIN, TONE_TILDE); return true;
        case 0x0168: setGlyph(spec, 'U', FORM_PLAIN, TONE_TILDE); return true;
        case 0x0169: setGlyph(spec, 'u', FORM_PLAIN, TONE_TILDE); return true;
        case 0x01A0: setGlyph(spec, 'O', FORM_HORN, TONE_NONE); return true;
        case 0x01A1: setGlyph(spec, 'o', FORM_HORN, TONE_NONE); return true;
        case 0x01AF: setGlyph(spec, 'U', FORM_HORN, TONE_NONE); return true;
        case 0x01B0: setGlyph(spec, 'u', FORM_HORN, TONE_NONE); return true;
        case 0x1EA0: setGlyph(spec, 'A', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EA1: setGlyph(spec, 'a', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EA2: setGlyph(spec, 'A', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EA3: setGlyph(spec, 'a', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EB8: setGlyph(spec, 'E', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EB9: setGlyph(spec, 'e', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EBA: setGlyph(spec, 'E', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EBB: setGlyph(spec, 'e', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EBC: setGlyph(spec, 'E', FORM_PLAIN, TONE_TILDE); return true;
        case 0x1EBD: setGlyph(spec, 'e', FORM_PLAIN, TONE_TILDE); return true;
        case 0x1EC8: setGlyph(spec, 'I', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EC9: setGlyph(spec, 'i', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1ECA: setGlyph(spec, 'I', FORM_PLAIN, TONE_DOT); return true;
        case 0x1ECB: setGlyph(spec, 'i', FORM_PLAIN, TONE_DOT); return true;
        case 0x1ECC: setGlyph(spec, 'O', FORM_PLAIN, TONE_DOT); return true;
        case 0x1ECD: setGlyph(spec, 'o', FORM_PLAIN, TONE_DOT); return true;
        case 0x1ECE: setGlyph(spec, 'O', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1ECF: setGlyph(spec, 'o', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EE4: setGlyph(spec, 'U', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EE5: setGlyph(spec, 'u', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EE6: setGlyph(spec, 'U', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EE7: setGlyph(spec, 'u', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EF2: setGlyph(spec, 'Y', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x1EF3: setGlyph(spec, 'y', FORM_PLAIN, TONE_GRAVE); return true;
        case 0x1EF4: setGlyph(spec, 'Y', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EF5: setGlyph(spec, 'y', FORM_PLAIN, TONE_DOT); return true;
        case 0x1EF6: setGlyph(spec, 'Y', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EF7: setGlyph(spec, 'y', FORM_PLAIN, TONE_HOOK); return true;
        case 0x1EF8: setGlyph(spec, 'Y', FORM_PLAIN, TONE_TILDE); return true;
        case 0x1EF9: setGlyph(spec, 'y', FORM_PLAIN, TONE_TILDE); return true;
        default: return false;
    }
}
uint8_t VN_LCD::acquireGlyph(uint32_t codepoint, const GlyphSpec& spec, bool& ok) {
    for (uint8_t i = 0; i < 8; i++) {
        if (_glyphSlots[i].used && _glyphSlots[i].codepoint == codepoint) {
            ok = true;
            return i;
        }
    }

    for (uint8_t i = 0; i < 8; i++) {
        if (!_glyphSlots[i].used) {
            buildGlyph(spec, _glyphSlots[i].pattern);
            loadCustomChar(i, _glyphSlots[i].pattern);
            _glyphSlots[i].codepoint = codepoint;
            _glyphSlots[i].used = true;
            ok = true;
            return i;
        }
    }

    ok = false;
    return static_cast<uint8_t>(spec.base);
}

void VN_LCD::buildGlyph(const GlyphSpec& spec, uint8_t pattern[8]) const {
    for (uint8_t row = 0; row < 8; row++) {
        pattern[row] = 0x00;
    }

    if (spec.form == FORM_STROKE) {
        if (spec.base == 'D') {
            pattern[0] = 0x0E;
            pattern[1] = 0x09;
            pattern[2] = 0x09;
            pattern[3] = 0x1D;
            pattern[4] = 0x09;
            pattern[5] = 0x09;
            pattern[6] = 0x0E;
        } else {
            pattern[0] = 0x01;
            pattern[1] = 0x05;
            pattern[2] = 0x1F;
            pattern[3] = 0x13;
            pattern[4] = 0x11;
            pattern[5] = 0x13;
            pattern[6] = 0x0D;
        }
        return;
    }

    if (spec.form == FORM_HORN && (spec.base == 'O' || spec.base == 'U')) {
        pattern[0] = 0x02;
        pattern[1] = 0x01;
        pattern[2] = spec.base == 'O' ? 0x0D : 0x01;
        pattern[3] = 0x12;
        pattern[4] = 0x12;
        pattern[5] = 0x12;
        pattern[6] = 0x0C;
        pattern[7] = spec.tone == TONE_DOT ? 0x04 : 0x00;
        return;
    }
    bool hasTopTone = spec.form == FORM_PLAIN && spec.tone != TONE_NONE && spec.tone != TONE_DOT;
    bool hasTopShape = spec.form == FORM_BREVE || spec.form == FORM_CIRCUMFLEX;
    bool hasSideShape = spec.form == FORM_HORN;
    bool compactBody = hasTopTone || hasTopShape || hasSideShape || spec.tone == TONE_DOT;
    uint8_t bodyStart = compactBody ? 3 : 2;
    uint8_t bodyRows = compactBody ? 4 : 5;

    const uint8_t* base = basePattern(spec.base, compactBody);
    for (uint8_t row = 0; row < bodyRows; row++) {
        pattern[bodyStart + row] = base[row];
    }

    switch (spec.form) {
        case FORM_CIRCUMFLEX:
            pattern[0] = 0x04;
            pattern[1] = 0x0A;
            break;
        case FORM_BREVE:
            pattern[0] = 0x11;
            pattern[1] = 0x0E;
            break;
        case FORM_HORN:
            pattern[1] = 0x03;
            pattern[2] = 0x01;
            pattern[bodyStart] |= 0x01;
            break;
        default:
            break;
    }

    switch (spec.tone) {
        case TONE_ACUTE:
            if (hasTopTone) {
                pattern[0] = 0x02;
                pattern[1] = 0x04;
            }
            break;
        case TONE_GRAVE:
            if (hasTopTone) {
                pattern[0] = 0x08;
                pattern[1] = 0x04;
            }
            break;
        case TONE_HOOK:
            if (hasTopTone) {
                pattern[0] = 0x0E;
                pattern[1] = 0x02;
            }
            break;
        case TONE_TILDE:
            if (hasTopTone) {
                pattern[0] = 0x0A;
                pattern[1] = 0x05;
            }
            break;
        case TONE_DOT:
            pattern[7] = 0x04;
            break;
        default:
            break;
    }
}
const uint8_t* VN_LCD::basePattern(char base, bool compact) const {
    static const uint8_t lowerA[5] = {0x0E, 0x01, 0x0F, 0x11, 0x0F};
    static const uint8_t lowerE[5] = {0x0E, 0x11, 0x1F, 0x10, 0x0E};
    static const uint8_t lowerI[5] = {0x00, 0x00, 0x04, 0x04, 0x0E};
    static const uint8_t lowerO[5] = {0x0E, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t lowerU[5] = {0x11, 0x11, 0x11, 0x13, 0x0D};
    static const uint8_t lowerY[5] = {0x11, 0x11, 0x0F, 0x01, 0x0E};

    static const uint8_t upperA[5] = {0x0E, 0x11, 0x1F, 0x11, 0x11};
    static const uint8_t upperE[5] = {0x1F, 0x10, 0x1E, 0x10, 0x1F};
    static const uint8_t upperI[5] = {0x0E, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t upperO[5] = {0x0E, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t upperU[5] = {0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t upperY[5] = {0x11, 0x0A, 0x04, 0x04, 0x04};

    static const uint8_t smallA[4] = {0x0E, 0x01, 0x0F, 0x0F};
    static const uint8_t smallE[4] = {0x0E, 0x1F, 0x10, 0x0E};
    static const uint8_t smallI[4] = {0x00, 0x04, 0x04, 0x0E};
    static const uint8_t smallO[4] = {0x0E, 0x11, 0x11, 0x0E};
    static const uint8_t smallU[4] = {0x11, 0x11, 0x13, 0x0D};
    static const uint8_t smallY[4] = {0x11, 0x0F, 0x01, 0x0E};

    static const uint8_t smallUpperA[4] = {0x0E, 0x11, 0x1F, 0x11};
    static const uint8_t smallUpperE[4] = {0x1F, 0x1E, 0x10, 0x1F};
    static const uint8_t smallUpperI[4] = {0x0E, 0x04, 0x04, 0x0E};
    static const uint8_t smallUpperO[4] = {0x0E, 0x11, 0x11, 0x0E};
    static const uint8_t smallUpperU[4] = {0x11, 0x11, 0x11, 0x0E};
    static const uint8_t smallUpperY[4] = {0x11, 0x0A, 0x04, 0x04};

    if (compact) {
        switch (base) {
            case 'a': return smallA;
            case 'e': return smallE;
            case 'i': return smallI;
            case 'o': return smallO;
            case 'u': return smallU;
            case 'y': return smallY;
            case 'A': return smallUpperA;
            case 'E': return smallUpperE;
            case 'I': return smallUpperI;
            case 'O': return smallUpperO;
            case 'U': return smallUpperU;
            case 'Y': return smallUpperY;
            default: return smallA;
        }
    }

    switch (base) {
        case 'a': return lowerA;
        case 'e': return lowerE;
        case 'i': return lowerI;
        case 'o': return lowerO;
        case 'u': return lowerU;
        case 'y': return lowerY;
        case 'A': return upperA;
        case 'E': return upperE;
        case 'I': return upperI;
        case 'O': return upperO;
        case 'U': return upperU;
        case 'Y': return upperY;
        default: return lowerA;
    }
}
void VN_LCD::loadCustomChar(uint8_t location, const uint8_t charmap[]) {
    location &= 0x07;
    uint8_t savedColumn = _cursorColumn;
    uint8_t savedRow = _cursorRow;

    command(LCD_SETCGRAMADDR | (location << 3));
    for (uint8_t i = 0; i < 8; i++) {
        send(charmap[i], PIN_REGISTER_SELECT);
    }

    setCursor(savedColumn, savedRow);
}
