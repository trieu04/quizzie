# Tài liệu Đặc tả Yêu cầu Phần mềm (Software Requirements Specification - SRS)

**Tên dự án:** Quizzie - Hệ thống Thi Trắc Nghiệm Trực Tuyến  
**Phiên bản:** 1.0  
**Ngày tạo:** 6/1/2026

---

## 1. Giới thiệu

### 1.1. Mục đích
Tài liệu này mô tả chi tiết các yêu cầu phần mềm cho hệ thống "Quizzie". Mục đích là cung cấp một bản đặc tả hoàn chỉnh về các chức năng, giao diện, và hiệu năng của hệ thống để đội ngũ phát triển, kiểm thử và các bên liên quan có thể tham chiếu.

### 1.2. Phạm vi sản phẩm
Quizzie là một hệ thống thi trắc nghiệm trực tuyến hoạt động theo mô hình Client-Server. Hệ thống cho phép người dùng đăng ký, đăng nhập, tổ chức các phòng thi (đối với Admin) và tham gia thi (đối với Participant) thông qua mạng LAN/Internet. Hệ thống tập trung vào tính năng thi thời gian thực, đồng bộ trạng thái và hiển thị kết quả xếp hạng ngay lập tức.

### 1.3. Định nghĩa và Viết tắt
- **Admin/Host**: Người dùng có quyền tạo phòng thi và quản lý bộ câu hỏi.
- **Participant**: Người dùng tham gia vào phòng thi để làm bài.
- **Client**: Ứng dụng chạy trên máy người dùng, có giao diện đồ họa (GUI).
- **Server**: Ứng dụng chạy trên máy chủ, xử lý logic trung tâm và lưu trữ dữ liệu.
- **GTK+**: Bộ công cụ widget để tạo giao diện đồ họa người dùng.
- **Poll**: Cơ chế I/O multiplexing được sử dụng trên Server để xử lý đa kết nối.

---

## 2. Mô tả tổng quan

### 2.1. Viễn cảnh sản phẩm
Quizzie là một hệ thống độc lập bao gồm:
- **Server Module**: Chạy trên môi trường Linux/Unix, chịu trách nhiệm quản lý kết nối, phòng thi và dữ liệu.
- **Client Module**: Chạy trên môi trường cài đặt sẵn thư viện GTK+, cung cấp giao diện tương tác cho người dùng.

Giao tiếp giữa Client và Server thông qua giao thức TCP/IP với định dạng bản tin văn bản (Text-based).

### 2.2. Đặc điểm người dùng
- **Người dùng phổ thông (Participant)**: Sử dụng hệ thống để ôn tập, thi cử. Yêu cầu giao diện đơn giản, dễ sử dụng.
- **Quản trị viên (Admin)**: Có kiến thức cơ bản về quản lý tệp tin để chuẩn bị ngân hàng câu hỏi (định dạng CSV) và cấu hình thông số phòng thi.

### 2.3. Môi trường vận hành
- **Server**:
  - Hệ điều hành: Linux (Ubuntu, CentOS, v.v.).
  - Kiến trúc: Đơn luồng (Single-thread) sử dụng `poll` cho I/O blocking.
- **Client**:
  - Hệ điều hành: Linux (hoặc các OS hỗ trợ GTK+ 3.0).
  - Thư viện yêu cầu: GTK+ 3.0.

---

## 3. Các yêu cầu cụ thể

### 3.1. Yêu cầu chức năng (Functional Requirements)

#### 3.1.1. Quản lý tài khoản và Phiên làm việc
- **REQ-AUTH-01 (Đăng ký)**: Người dùng có thể đăng ký tài khoản mới với tên đăng nhập (duy nhất) và mật khẩu.
- **REQ-AUTH-02 (Đăng nhập)**: Người dùng đăng nhập vào hệ thống để truy cập các chức năng.
- **REQ-AUTH-03 (Đăng xuất)**: Người dùng có thể đăng xuất khỏi hệ thống an toàn, giải phóng kết nối.
- **REQ-AUTH-04 (Tái kết nối)**: Hệ thống hỗ trợ cơ chế tự động kết nối lại (Rejoin) nếu mất kết nối mạng tạm thời.

#### 3.1.2. Quản lý Phòng thi (Dành cho Admin)
- **REQ-ROOM-01 (Tạo phòng)**: Admin có thể tạo phòng thi mới, thiết lập tên phòng, thời gian bắt đầu, thời gian kết thúc (hoặc thời lượng), số lượng câu hỏi, số lần thi cho phép.
- **REQ-ROOM-02 (Cấu hình đề thi)**: Admin chọn bộ câu hỏi từ ngân hàng câu hỏi có sẵn hoặc tải lên file CSV mới.
- **REQ-ROOM-03 (Quản lý phòng)**: Admin có thể xem danh sách phòng, xem chi tiết thống kê và xóa phòng đã tạo.
- **REQ-ROOM-04 (Quản lý câu hỏi)**: Cho phép Import (CSV)/Xem danh sách/Chỉnh sửa/Xóa ngân hàng câu hỏi. Client parse file CSV và gửi dữ liệu dạng JSON lên Server.
- **REQ-ROOM-05 (Xem chi tiết phòng)**: Admin có thể xem chi tiết cấu hình phòng, trạng thái hiện tại, thống kê tổng quát và danh sách kết quả của các thí sinh đã tham gia.

#### 3.1.3. Tham gia thi (Dành cho Participant)
- **REQ-QUIZ-01 (Xem danh sách phòng)**: Người dùng xem được danh sách các phòng thi đang mở, trạng thái phòng (Waiting, Running).
- **REQ-QUIZ-02 (Vào phòng)**: Người dùng tham gia vào một phòng chờ trước khi bài thi bắt đầu.
- **REQ-QUIZ-03 (Làm bài thi)**:
  - Hiển thị câu hỏi và 4 đáp án lựa chọn.
  - Đồng hồ đếm ngược thời gian còn lại.
  - Gửi đáp án đã chọn lên Server ngay lập tức để lưu trạng thái.
- **REQ-QUIZ-04 (Nộp bài)**: Người dùng có thể nộp bài trước khi hết giờ. Hệ thống tự động thu bài khi hết giờ.

#### 3.1.4. Chấm điểm và Xếp hạng
- **REQ-RANK-01 (Tính điểm)**: Server tự động tính điểm dựa trên số câu trả lời đúng.
- **REQ-RANK-02 (Bảng xếp hạng)**: Hiển thị bảng xếp hạng (Leaderboard) thời gian thực hoặc sau khi kết thúc bài thi, bao gồm tên người dùng và điểm số.

### 3.2. Yêu cầu phi chức năng (Non-functional Requirements)

#### 3.2.1. Hiệu năng (Performance)
- **REQ-PERF-01**: Server phải có khả năng xử lý đồng thời nhiều kết nối (Concurrent connections) mà không bị chặn (Non-blocking I/O).
- **REQ-PERF-02**: Độ trễ (Latency) khi gửi nhận câu trả lời phải thấp để đảm bảo tính thời gian thực của bài thi.

#### 3.2.2. Độ tin cậy (Reliability)
- **REQ-REL-01**: Server không được crash khi một client ngắt kết nối đột ngột.
- **REQ-REL-02**: Dữ liệu người dùng và câu hỏi phải được lưu trữ và đọc chính xác từ file.

#### 3.2.3. Khả năng bảo trì (Maintainability)
- **REQ-MAINT-01**: Code Client và Server được tách biệt rõ ràng theo module (Net, UI/Core, Storage).
- **REQ-MAINT-02**: Giao thức giao tiếp sử dụng JSON với header binary định rõ độ dài và loại tin nhắn, dễ dàng cho việc mở rộng và debug.

### 3.3. Yêu cầu giao diện (Interface Requirements)

#### 3.3.1. Giao diện người dùng (User Interface)
- Sử dụng thư viện **GTK+ 3.0**.
- **Màn hình Login**: 2 trường input (Username, Password) và nút Login/Register.
- **Màn hình Dashboard**: Danh sách phòng thi (List View), các nút chức năng (Create, Join, Output).
- **Màn hình Thi (Quiz)**: Khu vực hiển thị nội dung câu hỏi, 4 nút hoặc Radio button cho đáp án, thanh tiến trình hoặc đồng hồ đếm ngược.

#### 3.3.2. Giao diện giao tiếp (Communication Interface)
- Protocol: Binary Header + JSON Payload.
- Header:
    - `TotalLength` (4 bytes): Độ dài tổng cộng của gói tin (bao gồm header và payload).
    - `MSG_TYPE` (3 bytes): Loại tin nhắn (`REQ`, `RES`, `ERR`, `UPD`, `HBT`).
- Payload: Dữ liệu định dạng JSON, kích thước tối đa 128KB.
- Ví dụ:
    - Header: `00000100` (Length) + `REQ` (Type)
    - Payload: `{"action": "LOGIN", "data": {"username": "user1", "password": "123"}}`

---

## 4. Phụ lục: Định dạng dữ liệu

### 4.1. File Ngân hàng câu hỏi (CSV)
Định dạng:
```csv
Question_Text,Answer_A,Answer_B,Answer_C,Answer_D,Correct_Answer_Char
```
Ví dụ:
```csv
Thủ đô của Việt Nam là gì?,Hà Nội,TP.HCM,Đà Nẵng,Huế,A
```

### 4.2. File Người dùng
Lưu trữ thông tin đăng nhập (khuyến nghị mã hóa mật khẩu, nhưng phiên bản hiện tại có thể lưu plaintext cho mục đích học tập).
