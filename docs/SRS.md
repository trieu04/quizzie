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
- **cJSON**: Thư viện dùng để xử lý dữ liệu định dạng JSON trong C.

---

## 2. Mô tả tổng quan

### 2.1. Viễn cảnh sản phẩm
Quizzie là một hệ thống độc lập bao gồm:
- **Server Module**: Chạy trên môi trường Linux/Unix, chịu trách nhiệm quản lý kết nối, phòng thi và dữ liệu.
- **Client Module**: Chạy trên môi trường cài đặt sẵn thư viện GTK+, cung cấp giao diện tương tác cho người dùng.

Giao tiếp giữa Client và Server thông qua giao thức TCP/IP với định dạng gói tin có Header nhị phân và Payload JSON.

### 2.2. Đặc điểm người dùng
- **Người dùng phổ thông (Participant)**: Sử dụng hệ thống để ôn tập, thi cử. Yêu cầu giao diện đơn giản, dễ sử dụng.
- **Quản trị viên (Admin)**: Có quyền quản lý phòng thi, ngân hàng câu hỏi và xem thống kê kết quả.

### 2.3. Môi trường vận hành
- **Server**:
  - Hệ điều hành: Linux (Ubuntu, CentOS, v.v.).
  - Kiến trúc: Đơn luồng (Single-thread) sử dụng `poll` cho I/O multiplexing để xử lý đồng thời nhiều client.
- **Client**:
  - Hệ điều hành: Linux (hoặc các OS hỗ trợ GTK+ 3.0).
  - Thư viện yêu cầu: GTK+ 3.0.

---

## 3. Các yêu cầu cụ thể

### 3.1. Yêu cầu chức năng (Functional Requirements)

#### 3.1.1. Quản lý tài khoản và Phiên làm việc
- **REQ-AUTH-01 (Đăng ký)**: Người dùng có thể đăng ký tài khoản mới với tên đăng nhập (duy nhất), mật khẩu và vai trò (Admin/Participant).
- **REQ-AUTH-02 (Đăng nhập)**: Người dùng đăng nhập vào hệ thống. Hệ thống ngăn chặn việc đăng nhập đồng thời trên nhiều thiết bị bằng cùng một tài khoản (Kick duplicate login).
- **REQ-AUTH-03 (Đăng xuất)**: Người dùng có thể đăng xuất khỏi hệ thống an toàn.
- **REQ-AUTH-04 (Duy trì phiên)**: Server duy trì trạng thái đăng nhập cho client trong suốt thời gian kết nối.

#### 3.1.2. Quản lý Phòng thi (Dành cho Admin)
- **REQ-ROOM-01 (Tạo phòng)**: Admin tạo phòng thi với các thông số: tên phòng, thời gian bắt đầu, thời gian kết thúc, bộ câu hỏi, số lượng câu hỏi, thời lượng làm bài và số lần thi cho phép.
- **REQ-ROOM-02 (Tự động đóng phòng)**: Server tự động kiểm tra định kỳ (5 giây/lần) và chuyển trạng thái phòng từ `OPEN` sang `CLOSED` nếu đã quá thời gian kết thúc.
- **REQ-ROOM-03 (Quản lý phòng)**: Admin có thể xem danh sách phòng, xem chi tiết thống kê (số người tham gia, điểm trung bình) và xóa phòng.
- **REQ-ROOM-04 (Quản lý câu hỏi)**: Cho phép Import (CSV)/Xem danh sách/Xóa ngân hàng câu hỏi. Dữ liệu được lưu trữ dưới dạng JSON trên Server.
- **REQ-ROOM-05 (Xem kết quả)**: Admin có thể xem danh sách kết quả chi tiết của tất cả thí sinh trong một phòng thi.

#### 3.1.3. Tham gia thi (Dành cho Participant)
- **REQ-QUIZ-01 (Xem danh sách phòng)**: Người dùng xem được danh sách các phòng thi và trạng thái hiện tại.
- **REQ-QUIZ-02 (Vào phòng thi)**: Participant chỉ có thể tham gia nếu phòng đang `OPEN` và còn lượt thi.
- **REQ-QUIZ-03 (Làm bài thi)**:
  - Câu hỏi được trộn ngẫu nhiên (shuffle) từ ngân hàng câu hỏi trước khi gửi xuống Client.
  - Hiển thị nội dung câu hỏi và 4 lựa chọn (không bao gồm đáp án đúng để tránh gian lận).
  - Đồng hồ đếm ngược thời gian làm bài.
  - Gửi đáp án từng câu lên Server ngay khi chọn để lưu trạng thái.
- **REQ-QUIZ-04 (Phục hồi phiên thi)**: Nếu bị mất kết nối đột ngột, Participant có thể vào lại phòng thi để tiếp tục làm bài với các câu trả lời đã lưu và thời gian còn lại (Rejoin persistence).
- **REQ-QUIZ-05 (Nộp bài)**: Hệ thống tự động tính điểm và lưu kết quả vào file ngay khi kết thúc bài thi.

### 3.2. Yêu cầu phi chức năng (Non-functional Requirements)

#### 3.2.1. Hiệu năng (Performance)
- **REQ-PERF-01**: Server xử lý đồng thời nhiều kết nối sử dụng `poll` (Non-blocking I/O).
- **REQ-PERF-02**: Tốc độ phản hồi các yêu cầu từ Client phải đảm bảo trải nghiệm thi mượt mà.

#### 3.2.2. Độ tin cậy (Reliability)
- **REQ-REL-01**: Server hoạt động ổn định, xử lý các trường hợp Client ngắt kết nối bất thường mà không ảnh hưởng đến các client khác.
- **REQ-REL-02**: Cơ chế lưu trữ phiên thi (Session persistence) đảm bảo quyền lợi cho thí sinh khi gặp sự cố mạng.

#### 3.2.3. Khả năng bảo trì (Maintainability)
- **REQ-MAINT-01**: Kiến trúc phân lớp rõ ràng: Net (Mạng), Handlers (Xử lý logic), Storage (Lưu trữ).
- **REQ-MAINT-02**: Giao thức giao tiếp thống nhất: Binary Header + JSON Payload.

### 3.3. Yêu cầu giao diện (Interface Requirements)

#### 3.3.1. Giao diện người dùng (User Interface)
- Xây dựng trên nền tảng **GTK+ 3.0**.
- **Màn hình Dashboard**: Phân chia chức năng theo vai trò người dùng (Admin/Participant).

#### 3.3.2. Giao diện giao tiếp (Communication Interface)
- Protocol: Binary Header (7 bytes) + JSON Payload.
- Header:
    - `TotalLength` (4 bytes): Kiểu `uint32_t` (Network byte order), tổng độ dài gói tin.
    - `MSG_TYPE` (3 bytes): Chuỗi ký tự xác định loại tin nhắn (`REQ`, `RES`, `ERR`, `UPD`, `HBT`).
- Payload: Dữ liệu định dạng JSON xử lý qua thư viện `cJSON`.

---

## 4. Phụ lục: Lưu trữ dữ liệu

Hệ thống sử dụng cơ chế lưu trữ dạng file (Flat-file storage) trong thư mục `data/`:
- **Người dùng**: `data/users.txt` (định dạng `username:password:role`).
- **Phòng thi**: `data/rooms.json`.
- **Câu hỏi**: `data/questions/[bank_id].json`.
- **Phiên thi**: `data/sessions/` (lưu trạng thái thi hiện tại của thí sinh).
- **Kết quả**: `data/results/` (lưu điểm số cuối cùng).
