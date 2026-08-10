# 🚗 IoT Smart Parking System - Node 2 (Parking Slot Management)

## 📌 Giới thiệu dự án
Đây là mã nguồn và thiết kế phần cứng cho **Phân hệ Node 2 (Bãi đỗ xe thông minh)** thuộc dự án IoT quản lý bãi giữ xe và trạm sạc xe điện. Node 2 hoạt động như một Edge Node phân tán, đảm nhiệm việc thu thập dữ liệu môi trường, xử lý logic chống nhiễu tại chỗ (Edge Computing), báo cháy khẩn cấp và đồng bộ dữ liệu thời gian thực qua giao thức MQTT & ESP-NOW.

---

## 🎥 Video Demo Nguyên lý hoạt động
*Click vào ảnh dưới đây để xem video vận hành thực tế của hệ thống trên YouTube:*

[![Demo Hệ thống Bãi đỗ xe](https://img.youtube.com/vi/[thay-mã-ID-video-youtube-vào-đây]/0.jpg)](https://www.youtube.com/watch?v=[thay-mã-ID-video-youtube-vào-đây])

*(Ví dụ: Nếu link YouTube của bạn là `https://www.youtube.com/watch?v=dQw4w9WgXcQ`, hãy thay `dQw4w9WgXcQ` vào 2 vị trí trong ngoặc vuông ở trên).*

---

## 🖼️ Hình ảnh thực tế & Sơ đồ đấu dây

### Sơ đồ nguyên lý (Schematic)
![Sơ đồ đấu dây](schematic.png)
<img width="1536" height="1024" alt="schematic" src="https://github.com/user-attachments/assets/286e826e-9d65-4646-9778-0134020619d7" />

### Mô hình sa bàn thực tế (Physical Model)
![Ảnh sa bàn](physical-model.jpg)
<img width="3409" height="1922" alt="Physical Model" src="https://github.com/user-attachments/assets/76665667-e059-43dd-b125-bc5c14bcfa9c" />

---

## 🛠 Phần cứng sử dụng (Hardware)
*   **Vi điều khiển:** ESP32 DevKit V1
*   **Cảm biến đầu vào (Input):**
    *   Siêu âm HC-SR04 (Đo khoảng cách xe)
    *   Hồng ngoại PIR HC-SR501 (Phát hiện chuyển động)
    *   DHT11 (Đo nhiệt độ, độ ẩm vi khí hậu)
    *   Quang trở LDR (Đo cường độ ánh sáng)
    *   Flame Sensor (Cảm biến báo cháy)
    *   Nút nhấn khẩn cấp (Emergency Button)
*   **Thiết bị chấp hành (Output):**
    *   Màn hình LCD 16x2 I2C
    *   Relay 5V 1 kênh (Điều khiển đèn hầm chiếu sáng)
    *   LED 3 màu (Xanh/Vàng/Đỏ) báo trạng thái ô đỗ
    *   Active Buzzer (Còi báo động)

## 💻 Giao thức & Công nghệ phần mềm
*   **Ngôn ngữ:** C++ (Arduino Framework)
*   **Truyền thông nội bộ:** `ESP-NOW` (Đồng bộ dữ liệu khẩn cấp về Master Node với độ trễ < 50ms).
*   **Truyền thông Cloud:** `MQTT` (Truyền Telemetry và nhận lệnh RPC từ xa).
*   **Nền tảng IoT:** `ThingsBoard` (Xây dựng Dashboard giám sát và điều khiển trực quan).

## 🚀 Các tính năng cốt lõi & Giải thuật nổi bật
1.  **Lọc nhiễu "Ghost Car" (Thuật toán ưu tiên trạng thái):** Kết hợp cảm biến PIR và HC-SR04 để tránh hiện tượng nhận diện nhầm người đang thao tác thành xe ô tô. Tạm khóa quét siêu âm khi có chuyển động và kích hoạt "chốt hạ tức thời" khi người rời đi, triệt tiêu hoàn toàn lỗi đèn chớp ảo.
2.  **Đồng bộ công tắc 2 chiều (LDR & RPC):** Ngăn chặn xung đột lệnh giữa tính năng chiếu sáng hầm tự động (cảm biến ánh sáng LDR) và lệnh ghi đè thủ công từ Dashboard (RPC). Cảm biến chỉ kích hoạt Relay ở thời điểm giao thoa sáng/tối.
3.  **Quy trình ngắt khẩn cấp (PCCC):** Khi Flame sensor hoặc nút nhấn khẩn cấp được kích hoạt, hệ thống lập tức cắt điện Relay (ngừa chập cháy lan), khóa cứng màn hình LCD, chớp LED, hú còi liên tục và phát tín hiệu Broadcast báo động tới Master Node.
