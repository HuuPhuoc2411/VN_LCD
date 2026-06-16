
// Đây là code mẫu ví dụ giả lập hiển thị nhiệt độ lên màn hình
#include <VN_LCD.h>
const int LCD_SDA_PIN = 7;  // Khai báo chân SDA
const int LCD_SCL_PIN = 6;  // Khai báo chân SCL

// Tạo đối tượng LCD với địa chỉ I2C, số cột và số hàng của màn hình.
VN_LCD lcd(0x27, 16, 2);


float temperature = 25.0;
float direction = 0.2;

// Biến đánh dấu LCD đã khởi động thành công hay chưa.
bool lcdReady = false;


void setup() {

  Serial.begin(115200);
  delay(500);

  // Khởi động LCD bằng chân SDA/SCL đã khai báo và tự quét địa chỉ I2C nếu cần.
  lcdReady = lcd.beginWithScan(LCD_SDA_PIN, LCD_SCL_PIN, Serial);
  // Kiểm tra LCD đã sẵn sàng hay chưa.
  if (!lcdReady) {
    // Thoát khỏi hàm hiện tại nếu LCD chưa sẵn sàng.
    return;
  }

  lcd.backlight();                // Bật đèn nền LCD.
  lcd.clear();                    // Xóa toàn bộ màn hình LCD.
  lcd.setCursor(0, 0);            // Đặt con trỏ tại cột 0, hàng 0.
  lcd.print("*_* Xin chào *_*");  // In chữ lên màn hình LCD.
  lcd.setCursor(4, 1);            // Đặt con trỏ tại cột 4, hàng 1 để căn chữ ở dòng dưới.
  lcd.print("Việt Nam");
  delay(1500);
  lcd.clear();  // Xóa toàn bộ màn hình LCD.

}


void loop() {
  // Kiểm tra LCD đã sẵn sàng hay chưa.
  if (!lcdReady) {
    // Thoát khỏi hàm hiện tại nếu LCD chưa sẵn sàng.
    return;
  }
  lcd.setCursor(0, 0);  // Đặt con trỏ tại cột 0, hàng 0.
  lcd.print("nhiệt độ:");

  lcd.setCursor(10, 0);
  lcd.print(temperature, 1);

  lcd.print("0C");  // In ký hiệu độ C; thư viện tự đổi chuỗi 0C thành ký tự đặc biệt.

  delay(1000);

  // Xóa đúng 4 ô tại vùng hiển thị nhiệt độ, không xóa cả màn hình để tránh bị chớp.
  lcd.clear(10, 0, 4);

  // Đoạn này để giả lập nhiệt độ đang thay đổi
  temperature += direction;
  if (temperature >= 30.0) {
    temperature = 30.0;
    direction = -0.2;
  } else if (temperature <= 25.0) {
    temperature = 25.0;
    direction = 0.2;
  }
}
