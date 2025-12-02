# Tài liệu Đặc tả Yêu cầu Phần mềm (SRS)
# Hệ thống Quizzie - Thi trắc nghiệm trực tuyến

**Phiên bản:** 2.0  
**Ngày cập nhật:** 02/12/2025  
**Tác giả:** Nhóm phát triển Quizzie

---

## Mục lục

1. [Giới thiệu](#1-giới-thiệu)
2. [Mô tả tổng quan](#2-mô-tả-tổng-quan)
3. [Yêu cầu chức năng](#3-yêu-cầu-chức-năng)
4. [Yêu cầu phi chức năng](#4-yêu-cầu-phi-chức-năng)
5. [Kiến trúc hệ thống](#5-kiến-trúc-hệ-thống)
6. [Giao thức truyền thông](#6-giao-thức-truyền-thông)
7. [Giao diện người dùng](#7-giao-diện-người-dùng)
8. [Cấu trúc dữ liệu](#8-cấu-trúc-dữ-liệu)
9. [Ràng buộc và giới hạn](#9-ràng-buộc-và-giới-hạn)
10. [Danh sách chức năng chi tiết](#10-danh-sách-chức-năng-chi-tiết)

---

## 1. Giới thiệu

### 1.1 Mục đích
Tài liệu này mô tả chi tiết các yêu cầu phần mềm cho hệ thống **Quizzie** - một ứng dụng thi trắc nghiệm trực tuyến trên nền tảng Console/Terminal. Tài liệu cung cấp thông tin cho đội phát triển, kiểm thử và các bên liên quan.

### 1.2 Phạm vi
Quizzie là hệ thống thi trắc nghiệm cho phép:
- Người dùng đăng nhập và xác thực
- Luyện tập cá nhân với bộ câu hỏi
- Tạo phòng thi và thi đấu với người dùng khác trong thời gian thực
- Xem kết quả và thống kê điểm số

### 1.3 Định nghĩa và thuật ngữ

| Thuật ngữ | Định nghĩa |
|-----------|------------|
| TUI | Terminal User Interface - Giao diện người dùng trên terminal |
| Ncurses | Thư viện C để xây dựng giao diện TUI |
| TCP/IP | Bộ giao thức truyền thông mạng |
| epoll | Cơ chế I/O multiplexing trên Linux |
| Non-blocking I/O | Cơ chế I/O không chặn luồng xử lý |
| Host | Người tạo phòng thi |
| Client | Ứng dụng phía người dùng |
| Server | Ứng dụng điều phối trung tâm |

### 1.4 Tài liệu tham khảo
- Linux epoll man pages
- Ncurses Programming HOWTO
- Beej's Guide to Network Programming

---

## 2. Mô tả tổng quan

### 2.1 Góc nhìn sản phẩm
Quizzie là một hệ thống client-server độc lập, hoạt động trên nền tảng Linux. Hệ thống giao tiếp qua mạng TCP/IP và hiển thị giao diện trên Terminal.

### 2.2 Chức năng chính

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            QUIZZIE SYSTEM                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────────────────── SERVER ─────────────────────────────┐   │
│  │                                                                      │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────┐  │   │
│  │  │ Socket I/O  │  │  Quản lý    │  │  Quản lý    │  │   Chấm     │  │   │
│  │  │   (epoll)   │  │  Tài khoản  │  │  Phòng thi  │  │   điểm     │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └────────────┘  │   │
│  │         │                │                │               │         │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────┐  │   │
│  │  │   Stream    │  │   Phiên     │  │  Phân loại  │  │   Ghi      │  │   │
│  │  │  Handling   │  │  làm việc   │  │  câu hỏi    │  │   Log      │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └────────────┘  │   │
│  │                                                                      │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                    ▲                                        │
│                                    │ TCP/IP                                 │
│                                    ▼                                        │
│  ┌──────────────────────────────── CLIENT ─────────────────────────────┐   │
│  │                                                                      │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────┐  │   │
│  │  │   Đăng nhập │  │  Tạo phòng  │  │   Tham gia  │  │   Làm      │  │   │
│  │  │  Kết nối    │  │    thi      │  │   phòng     │  │   bài      │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └────────────┘  │   │
│  │         │                │                │               │         │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────┐  │   │
│  │  │   Xem       │  │  Host Panel │  │   Thống     │  │   Xem      │  │   │
│  │  │  danh sách  │  │  (Quản lý)  │  │    kê       │  │  kết quả   │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └────────────┘  │   │
│  │                                                                      │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Đặc điểm người dùng
- **Người dùng thông thường**: Sử dụng terminal cơ bản, tham gia thi trắc nghiệm
- **Host (Chủ phòng)**: Người tạo phòng và khởi động bài thi

### 2.4 Môi trường hoạt động
- **Hệ điều hành:** Linux (Ubuntu, Debian, Fedora, ...)
- **Yêu cầu:** GCC compiler, thư viện Ncurses, kết nối mạng
- **Terminal:** Hỗ trợ Unicode và màu sắc (optional)

---

## 3. Yêu cầu chức năng

### 3.1 Module Xác thực (Authentication)

#### FR-AUTH-001: Đăng nhập
- **Mô tả:** Người dùng nhập username và password để đăng nhập
- **Đầu vào:** Username (tối đa 31 ký tự), Password (tối đa 31 ký tự)
- **Xử lý:** 
  1. Validate đầu vào không rỗng
  2. Kết nối đến server
  3. Lưu thông tin phiên đăng nhập
- **Đầu ra:** Chuyển đến Dashboard nếu thành công, hiển thị lỗi nếu thất bại
- **Độ ưu tiên:** Cao

#### FR-AUTH-002: Đăng xuất
- **Mô tả:** Người dùng đăng xuất khỏi hệ thống
- **Xử lý:** Đóng kết nối, xóa thông tin phiên
- **Đầu ra:** Quay về màn hình đăng nhập
- **Độ ưu tiên:** Cao

#### FR-AUTH-003: Kết nối lại (Reconnect)
- **Mô tả:** Cho phép host kết nối lại phòng thi đã tạo sau khi mất kết nối
- **Xử lý:**
  1. Xác định phòng thi dựa trên username
  2. Khôi phục trạng thái host
  3. Cập nhật socket mới cho host
- **Đầu ra:** Quay lại Host Panel với trạng thái phòng hiện tại
- **Độ ưu tiên:** Cao

### 3.2 Module Dashboard

#### FR-DASH-001: Hiển thị Dashboard
- **Mô tả:** Hiển thị menu chính sau khi đăng nhập
- **Thành phần:**
  - Thông tin người dùng đang đăng nhập
  - Trạng thái kết nối server
  - Menu lựa chọn: Tạo phòng / Xem danh sách phòng / Đăng xuất
- **Độ ưu tiên:** Cao

#### FR-DASH-002: Tạo phòng thi
- **Mô tả:** Người dùng tạo phòng thi mới với cấu hình tùy chỉnh
- **Xử lý:**
  1. Gửi yêu cầu CREATE_ROOM đến server với cấu hình (thời gian, file câu hỏi)
  2. Server load câu hỏi và tạo phòng
  3. Nhận Room ID từ server
  4. Trở thành Host của phòng
- **Đầu ra:** Chuyển đến Host Panel với vai trò Host
- **Độ ưu tiên:** Cao

#### FR-DASH-003: Xem danh sách phòng thi
- **Mô tả:** Người dùng xem danh sách các phòng đang hoạt động
- **Xử lý:** 
  1. Gửi yêu cầu LIST_ROOMS đến server
  2. Nhận và parse danh sách phòng
  3. Hiển thị với thông tin: Room ID, Host, số người chơi, trạng thái
- **Đầu ra:** Chuyển đến màn hình Room List
- **Độ ưu tiên:** Cao

### 3.3 Module Quản lý Phòng thi (Host Panel)

#### FR-ROOM-001: Host Panel - Quản lý phòng
- **Mô tả:** Giao diện điều khiển dành riêng cho Host
- **Thành phần:**
  - Thông tin phòng (ID, trạng thái)
  - Cấu hình quiz (thời gian, file câu hỏi)
  - Danh sách và trạng thái thí sinh
  - Thống kê real-time
- **Độ ưu tiên:** Cao

#### FR-ROOM-002: Cấu hình bài thi
- **Mô tả:** Host có thể thay đổi cấu hình trước khi bắt đầu
- **Cấu hình:**
  - Thời gian làm bài (giây)
  - File câu hỏi sử dụng
- **Điều kiện:** Chỉ có thể thay đổi khi quiz chưa bắt đầu
- **Độ ưu tiên:** Cao

#### FR-ROOM-003: Khởi động bài thi
- **Mô tả:** Host khởi động bài thi cho tất cả người chơi
- **Điều kiện:** Chỉ Host mới có quyền
- **Xử lý:**
  1. Gửi yêu cầu START_GAME đến server
  2. Server thông báo QUIZ_AVAILABLE đến tất cả thí sinh
  3. Mỗi thí sinh tự bắt đầu khi sẵn sàng
- **Đầu ra:** Quiz chuyển sang trạng thái STARTED
- **Độ ưu tiên:** Cao

#### FR-ROOM-004: Xem thống kê real-time
- **Mô tả:** Host xem trạng thái thí sinh theo thời gian thực
- **Thông tin hiển thị:**
  - Số người đang chờ / đang làm / đã nộp
  - Chi tiết từng thí sinh: username, trạng thái, thời gian còn lại/điểm số
- **Độ ưu tiên:** Cao

### 3.4 Module Tham gia phòng thi

#### FR-JOIN-001: Nhập Room ID để tham gia
- **Mô tả:** Người dùng nhập ID phòng để tham gia
- **Đầu vào:** Room ID (số nguyên dương)
- **Xử lý:**
  1. Validate Room ID
  2. Gửi yêu cầu JOIN_ROOM đến server kèm username
  3. Nhận phản hồi với thông tin phòng (thời gian, trạng thái)
- **Đầu ra:** Vào phòng nếu thành công, thông báo lỗi nếu thất bại
- **Độ ưu tiên:** Cao

#### FR-JOIN-002: Chờ Host bắt đầu
- **Mô tả:** Thí sinh chờ đợi Host khởi động quiz
- **Xử lý:** Lắng nghe message QUIZ_AVAILABLE từ server
- **Đầu ra:** Hiển thị nút "Start Quiz" khi quiz sẵn sàng
- **Độ ưu tiên:** Cao

#### FR-JOIN-003: Bắt đầu làm bài cá nhân
- **Mô tả:** Thí sinh bắt đầu làm bài khi sẵn sàng
- **Xử lý:**
  1. Gửi CLIENT_START đến server
  2. Server bắt đầu đếm giờ cho thí sinh này
  3. Nhận câu hỏi từ server
- **Đầu ra:** Chuyển đến màn hình Quiz với câu hỏi
- **Độ ưu tiên:** Cao

### 3.5 Module Thi trắc nghiệm

#### FR-QUIZ-001: Hiển thị câu hỏi
- **Mô tả:** Hiển thị câu hỏi và 4 đáp án (A, B, C, D)
- **Thành phần:**
  - Sidebar: Thông tin phòng, tiến độ, danh sách câu hỏi với trạng thái đã chọn
  - Main: Nội dung câu hỏi và các lựa chọn
  - Timer: Thời gian còn lại (nếu có giới hạn)
- **Độ ưu tiên:** Cao

#### FR-QUIZ-002: Chọn đáp án
- **Mô tả:** Người dùng chọn đáp án bằng phím A, B, C, D
- **Xử lý:** Lưu đáp án vào bộ nhớ client
- **Đầu ra:** Highlight đáp án đã chọn
- **Độ ưu tiên:** Cao

#### FR-QUIZ-003: Thay đổi đáp án
- **Mô tả:** Cho phép thay đổi đáp án đã chọn trước khi nộp
- **Xử lý:** Cập nhật đáp án trong bộ nhớ client
- **Đầu ra:** Cập nhật highlight cho đáp án mới
- **Độ ưu tiên:** Cao

#### FR-QUIZ-004: Điều hướng câu hỏi
- **Mô tả:** Di chuyển giữa các câu hỏi
- **Phím tắt:** LEFT/RIGHT arrows, click vào sidebar
- **Độ ưu tiên:** Cao

#### FR-QUIZ-005: Nộp bài
- **Mô tả:** Gửi tất cả đáp án đến server để chấm điểm
- **Phím tắt:** S
- **Xử lý:**
  1. Tạo chuỗi đáp án
  2. Gửi SUBMIT đến server
  3. Server tính điểm và thời gian làm bài
  4. Nhận kết quả
- **Đầu ra:** Chuyển đến màn hình kết quả
- **Độ ưu tiên:** Cao

#### FR-QUIZ-006: Thoát bài thi
- **Mô tả:** Rời khỏi bài thi đang làm
- **Phím tắt:** ESC
- **Xử lý:** Reset trạng thái quiz, quay về danh sách phòng
- **Độ ưu tiên:** Trung bình

### 3.6 Module Kết quả

#### FR-RESULT-001: Hiển thị kết quả
- **Mô tả:** Hiển thị điểm số và đánh giá
- **Thành phần:**
  - Điểm: X/Y (số câu đúng/tổng số câu)
  - Phần trăm
  - Thời gian làm bài
  - Đánh giá dựa trên điểm
- **Độ ưu tiên:** Cao

#### FR-RESULT-002: Chơi lại hoặc quay về
- **Mô tả:** Lựa chọn sau khi xem kết quả
- **Options:** Play Again / Dashboard
- **Độ ưu tiên:** Trung bình

#### FR-RESULT-003: Thống kê bài thi
- **Mô tả:** Hiển thị thống kê chi tiết về bài thi
- **Thành phần:**
  - Thời gian hoàn thành
  - So sánh với thời gian cho phép
  - Số câu đúng/sai/bỏ qua
- **Độ ưu tiên:** Trung bình

---

## 4. Yêu cầu phi chức năng

### 4.1 Hiệu năng

#### NFR-PERF-001: Thời gian phản hồi
- Thời gian phản hồi UI: < 100ms
- Thời gian nhận tin nhắn từ server: < 500ms (trên mạng LAN)

#### NFR-PERF-002: Khả năng đồng thời
- Server hỗ trợ tối đa **100 client** đồng thời
- Tối đa **10 phòng thi** cùng lúc

#### NFR-PERF-003: Sử dụng tài nguyên
- Sử dụng Non-blocking I/O để không block luồng chính
- Sử dụng epoll để xử lý nhiều kết nối hiệu quả

### 4.2 Độ tin cậy

#### NFR-REL-001: Xử lý ngắt kết nối
- Client phát hiện và thông báo khi mất kết nối server
- Server xử lý client disconnect gracefully

#### NFR-REL-002: Xử lý lỗi
- Tất cả lỗi được log với thông tin chi tiết
- Thông báo lỗi user-friendly trên UI

### 4.3 Tính khả dụng

#### NFR-USA-001: Giao diện thân thiện
- Sử dụng phím tắt trực quan
- Hiển thị hướng dẫn ở footer mỗi màn hình
- Hỗ trợ điều hướng bằng chuột

#### NFR-USA-002: Phản hồi trực quan
- Highlight lựa chọn đang focus
- Hiển thị trạng thái kết nối
- Thông báo trạng thái các thao tác

### 4.4 Tính bảo trì

#### NFR-MAIN-001: Cấu trúc mã nguồn
- Tách biệt các module: Core, Network, UI, Storage
- Sử dụng header files để khai báo interface
- Comment đầy đủ cho các hàm public

#### NFR-MAIN-002: Build system
- Sử dụng Makefile chuẩn
- Hỗ trợ build tăng dần (incremental build)
- Clean build dễ dàng

---

## 5. Kiến trúc hệ thống

### 5.1 Mô hình tổng quan

```
┌─────────────────────────────────────────────────────────────────┐
│                         QUIZZIE SYSTEM                          │
│                                                                 │
│    ┌──────────────┐         TCP/IP          ┌──────────────┐   │
│    │   CLIENT 1   │◄──────────────────────►│              │   │
│    └──────────────┘                         │              │   │
│                                             │              │   │
│    ┌──────────────┐         TCP/IP          │   SERVER     │   │
│    │   CLIENT 2   │◄──────────────────────►│   (epoll)    │   │
│    └──────────────┘                         │              │   │
│                                             │              │   │
│    ┌──────────────┐         TCP/IP          │              │   │
│    │   CLIENT N   │◄──────────────────────►│              │   │
│    └──────────────┘                         └──────┬───────┘   │
│                                                    │           │
│                                             ┌──────▼───────┐   │
│                                             │  questions.  │   │
│                                             │     txt      │   │
│                                             └──────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Kiến trúc Server

```
┌─────────────────────────────────────────────────────┐
│                      SERVER                         │
├─────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────┐   │
│  │               MAIN (main.c)                  │   │
│  │  - Entry point                              │   │
│  │  - Khởi tạo và chạy server                  │   │
│  └─────────────────────────────────────────────┘   │
│                       │                             │
│  ┌───────────────────────────────────────────────┐ │
│  │              CORE (server.c)                   │ │
│  │  - server_init(): Khởi tạo context            │ │
│  │  - server_run(): Event loop chính             │ │
│  │  - server_cleanup(): Dọn dẹp tài nguyên       │ │
│  └───────────────────────────────────────────────┘ │
│           │                    │                    │
│  ┌────────▼────────┐  ┌───────▼────────┐          │
│  │   NET (net.c)   │  │ ROOM (room.c)  │          │
│  │  - net_setup()  │  │ - room_create()│          │
│  │  - net_accept() │  │ - room_join()  │          │
│  │  - net_send()   │  │ - room_start() │          │
│  │  - net_receive()│  │ - room_submit()│          │
│  └─────────────────┘  └────────────────┘          │
│           │                    │                    │
│  ┌────────▼────────────────────▼────────┐          │
│  │         STORAGE (storage.c)          │          │
│  │  - storage_load_questions()          │          │
│  │  - storage_save_log()                │          │
│  └──────────────────────────────────────┘          │
└─────────────────────────────────────────────────────┘
```

### 5.3 Kiến trúc Client

```
┌─────────────────────────────────────────────────────┐
│                      CLIENT                         │
├─────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────┐   │
│  │               MAIN (main.c)                  │   │
│  │  - Entry point                              │   │
│  │  - Khởi tạo UI và client                    │   │
│  └─────────────────────────────────────────────┘   │
│                       │                             │
│  ┌───────────────────────────────────────────────┐ │
│  │              CORE (client.c)                   │ │
│  │  - client_init(): Khởi tạo context            │ │
│  │  - client_run(): Main loop + state machine    │ │
│  │  - client_send_message(): Gửi tin             │ │
│  │  - client_process_message(): Xử lý response   │ │
│  └───────────────────────────────────────────────┘ │
│           │                    │                    │
│  ┌────────▼────────┐  ┌───────▼────────────────┐  │
│  │   NET (net.c)   │  │      UI (ui.c)         │  │
│  │  - net_connect()│  │  - ui_init()           │  │
│  │  - net_send()   │  │  - ui_cleanup()        │  │
│  │  - net_receive()│  │  - ui_get_input()      │  │
│  │  - net_close()  │  └───────┬────────────────┘  │
│  └─────────────────┘          │                    │
│                      ┌────────▼────────────────┐   │
│                      │    PAGES (pages/)       │   │
│                      │  - login.c              │   │
│                      │  - dashboard.c          │   │
│                      │  - room_list.c          │   │
│                      │  - host_panel.c (NEW)   │   │
│                      │  - quiz.c               │   │
│                      │  - result.c             │   │
│                      └─────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

### 5.4 State Machine (Client)

```
                        ┌─────────────┐
                        │   START     │
                        └──────┬──────┘
                               │
                        ┌──────▼──────┐
                ┌───────│ PAGE_LOGIN  │◄─────────────────────┐
                │       └──────┬──────┘                      │
                │              │ Login success               │ Logout
                │       ┌──────▼──────┐                      │
                │       │PAGE_DASHBOARD│──────────────────────┘
                │       └───┬─────┬───┘
                │           │     │
        Create Room         │     │ View Rooms
                │    ┌──────▼─────▼──────────────────┐
                │    │                                │
        ┌───────▼────▼────┐                  ┌───────▼────────┐
        │  PAGE_HOST_PANEL│◄─────────────────│ PAGE_ROOM_LIST │
        │  (Host control) │  Rejoin as Host  └───────┬────────┘
        └───────┬─────────┘                          │
                │                                    │ Join Room
                │                            ┌───────▼────────┐
                │                            │   PAGE_QUIZ    │
                │                            │  (Participant) │
                │                            └───────┬────────┘
                │                                    │ Submit
                │                            ┌───────▼────────┐
                └───────────────────────────►│  PAGE_RESULT   │
                    (View result)            └───────┬────────┘
                                                     │
                                                     ▼
                                    Play Again / Dashboard
```

---

## 6. Giao thức truyền thông

### 6.1 Định dạng tin nhắn
Tất cả tin nhắn tuân theo format: `TYPE:DATA`

### 6.2 Các loại tin nhắn

#### 6.2.1 Client → Server

| Type | Data | Mô tả |
|------|------|-------|
| `CREATE_ROOM` | `username[,duration,filename]` | Yêu cầu tạo phòng mới (với cấu hình tùy chọn) |
| `JOIN_ROOM` | `room_id,username` | Yêu cầu vào phòng |
| `LIST_ROOMS` | (empty) | Lấy danh sách phòng |
| `REJOIN_HOST` | `username` | Host kết nối lại phòng đã tạo |
| `SET_CONFIG` | `duration,filename` | Cập nhật cấu hình phòng |
| `START_GAME` | (empty) | Host yêu cầu bắt đầu quiz |
| `CLIENT_START` | (empty) | Thí sinh bắt đầu làm bài |
| `GET_STATS` | (empty) | Host lấy thống kê phòng |
| `SUBMIT` | `answers` | Nộp đáp án (VD: "ABCDA") |

#### 6.2.2 Server → Client

| Type | Data | Mô tả |
|------|------|-------|
| `ROOM_CREATED` | `room_id,duration` | Phòng đã tạo thành công |
| `ROOM_REJOINED` | `room_id,duration,state` | Đã kết nối lại phòng |
| `JOINED` | `room_id,duration,state` | Đã vào phòng thành công |
| `ROOM_LIST` | `id,host,count,state;...` | Danh sách phòng |
| `CONFIG_UPDATED` | `duration,filename` | Cấu hình đã cập nhật |
| `QUIZ_AVAILABLE` | `duration` | Quiz đã sẵn sàng (gửi đến thí sinh) |
| `QUIZ_STARTED` | `participant_count` | Xác nhận quiz bắt đầu (gửi đến host) |
| `QUESTIONS` | `duration;Q1?opts;Q2?...` | Danh sách câu hỏi |
| `ROOM_STATS` | `waiting,taking,submitted;participants` | Thống kê phòng |
| `PARTICIPANT_JOINED` | `username` | Thí sinh mới vào phòng |
| `PARTICIPANT_STARTED` | `username` | Thí sinh bắt đầu làm bài |
| `PARTICIPANT_SUBMITTED` | `username,score,total` | Thí sinh nộp bài |
| `RESULT` | `score/total,time_taken` | Kết quả chấm điểm |
| `ERROR` | `message` | Thông báo lỗi |

### 6.3 Format câu hỏi

```
QuestionText?A.opt1|B.opt2|C.opt3|D.opt4;NextQuestion?...
```

**Ví dụ:**
```
Thủ đô của Việt Nam?A.Hà Nội|B.HCM|C.Đà Nẵng|D.Huế;2+2=?A.3|B.4|C.5|D.6
```

### 6.4 Format thống kê phòng (ROOM_STATS)

```
waiting,taking,submitted;user1:status:value,user2:status:value,...
```

**Trong đó:**
- `status`: W (waiting), T (taking), S (submitted)
- `value`: thời gian còn lại (nếu T), điểm/tổng (nếu S)

**Ví dụ:**
```
ROOM_STATS:1,2,3;user1:W:0,user2:T:120,user3:S:4/5
```

---

## 7. Giao diện người dùng

### 7.1 Màn hình Login

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│                    QUIZZIE - Login                           │
│                                                              │
│                   [Connected to server]                      │
│                                                              │
│                   Username: [______________]                 │
│                   Password: [**************]                 │
│                                                              │
│                        [ LOGIN ]                             │
│                                                              │
│                                                              │
│  TAB: Switch focus | ENTER: Select | F10: Quit              │
└──────────────────────────────────────────────────────────────┘
```

### 7.2 Màn hình Dashboard

```
┌──────────────────────────────────────────────────────────────┐
│                        DASHBOARD                             │
│                                                              │
│                   Welcome, username!                         │
│                [Connected to 127.0.0.1:8080]                 │
│                                                              │
│                    [ Create Room ]                           │
│                    [ Join Room   ]                           │
│                    [ Logout      ]                           │
│                                                              │
│                                                              │
│  UP/DOWN: Select | ENTER: Confirm | F10: Quit               │
└──────────────────────────────────────────────────────────────┘
```

### 7.3 Màn hình Quiz

```
┌──────────────┬───────────────────────────────────────────────┐
│ QUIZ         │                                               │
│              │  Question 1:                                  │
│ Room: 1      │                                               │
│ Player: user │  Thủ đô của Việt Nam là gì?                  │
│ [HOST]       │                                               │
│              │  [A] Hà Nội                                   │
│ Progress:    │  [B] HCM                                      │
│ 1 / 5        │  [C] Hải Phòng                               │
│              │  [D] Đà Nẵng                                  │
│ Questions:   │                                               │
│   Q1 [A]     │                                               │
│   Q2 [-]     │  Selected: A                                  │
│   Q3 [-]     │                                               │
│   Q4 [-]     │                                               │
│   Q5 [-]     │                                               │
├──────────────┴───────────────────────────────────────────────┤
│  A-D: Answer | LEFT/RIGHT: Nav | S: Submit | ESC: Leave     │
└──────────────────────────────────────────────────────────────┘
```

### 7.4 Màn hình Result

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│                      QUIZ RESULTS                            │
│                                                              │
│                       Your Score:                            │
│                         4 / 5                                │
│                                                              │
│                    Percentage: 80.0%                         │
│                                                              │
│                 Excellent! Great job!                        │
│                                                              │
│                    [ Play Again ]                            │
│                    [ Dashboard  ]                            │
│                                                              │
│  UP/DOWN: Select | ENTER: Confirm | F10: Quit               │
└──────────────────────────────────────────────────────────────┘
```

---

## 8. Cấu trúc dữ liệu

### 8.1 Server Structures

#### 8.1.1 Quiz State Enum
```c
typedef enum {
    QUIZ_STATE_WAITING,    // Chờ host bắt đầu
    QUIZ_STATE_STARTED,    // Quiz đang diễn ra
    QUIZ_STATE_FINISHED    // Quiz đã kết thúc
} QuizState;
```

#### 8.1.2 Client Structure (Server-side)
```c
typedef struct {
    int sock;                  // Socket descriptor
    char username[50];         // Tên người dùng
    time_t quiz_start_time;    // Thời điểm bắt đầu làm bài
    bool is_taking_quiz;       // Đang làm bài hay chưa
    bool has_submitted;        // Đã nộp bài chưa
    int score;                 // Điểm số
    int total;                 // Tổng số câu hỏi
} Client;
```

#### 8.1.3 Room Structure
```c
typedef struct {
    int id;                         // Room ID
    char host_username[50];         // Username của host (để hỗ trợ reconnect)
    int host_sock;                  // Socket hiện tại của host
    Client *clients[MAX_CLIENTS];   // Danh sách thí sinh trong phòng
    int client_count;               // Số lượng thí sinh
    char questions[BUFFER_SIZE];    // Chuỗi câu hỏi
    char correct_answers[50];       // Đáp án đúng
    int quiz_duration;              // Thời gian làm bài (giây)
    QuizState state;                // Trạng thái quiz
    char question_file[128];        // File câu hỏi được chọn
} Room;
```

#### 8.1.4 Message Structure
```c
typedef struct {
    char type[20];     // Loại tin nhắn
    char data[1000];   // Dữ liệu tin nhắn
} Message;
```

#### 8.1.5 Server Context
```c
typedef struct ServerContext {
    int server_fd;              // Server socket
    int epoll_fd;               // Epoll instance
    Client clients[MAX_CLIENTS]; // Tất cả clients đã kết nối
    Room rooms[MAX_ROOMS];       // Tất cả phòng thi
    int client_count;           // Tổng số clients
    int room_count;             // Tổng số phòng
    bool running;               // Trạng thái server
} ServerContext;
```

### 8.2 Client Structures

#### 8.2.1 Application State Enum
```c
typedef enum {
    PAGE_LOGIN,       // Màn hình đăng nhập
    PAGE_DASHBOARD,   // Menu chính
    PAGE_ROOM_LIST,   // Danh sách phòng
    PAGE_QUIZ,        // Làm bài thi
    PAGE_RESULT,      // Kết quả
    PAGE_HOST_PANEL   // Panel điều khiển của Host
} AppState;
```

#### 8.2.2 Quiz State Enum (Client-side)
```c
typedef enum {
    QUIZ_STATE_WAITING = 0,   // Chờ host bắt đầu
    QUIZ_STATE_STARTED = 1,   // Quiz đang diễn ra
    QUIZ_STATE_FINISHED = 2   // Quiz đã kết thúc
} QuizState;
```

#### 8.2.3 Question Structure
```c
typedef struct {
    int id;                  // ID câu hỏi
    char question[256];      // Nội dung câu hỏi
    char options[4][128];    // 4 lựa chọn (A, B, C, D)
    char correct_answer;     // Đáp án đúng
} Question;
```

#### 8.2.4 Room Info Structure (for Room List)
```c
typedef struct {
    int id;                    // Room ID
    char host_username[32];    // Username của host
    int player_count;          // Số người chơi
    int state;                 // Trạng thái (0: waiting, 1: started, 2: finished)
    bool is_my_room;           // True nếu user hiện tại là host
} RoomInfo;
```

#### 8.2.5 Participant Info Structure (for Host Panel)
```c
typedef struct {
    char username[50];     // Tên thí sinh
    char status;           // 'W' = waiting, 'T' = taking, 'S' = submitted
    int remaining_time;    // Thời gian còn lại (khi đang làm)
    int score;             // Điểm (khi đã nộp)
    int total;             // Tổng số câu (khi đã nộp)
} ParticipantInfo;
```

#### 8.2.6 Client Context
```c
typedef struct ClientContext {
    // Network
    int socket_fd;                  // Socket đến server
    struct sockaddr_in server_addr; // Địa chỉ server
    bool running;                   // Ứng dụng đang chạy
    bool connected;                 // Trạng thái kết nối
    AppState current_state;         // Trang hiện tại
    
    // User state
    char username[32];              // Tên người dùng hiện tại
    
    // Room state
    int current_room_id;            // Room ID hiện tại
    bool is_host;                   // Có phải host không
    QuizState room_state;           // Trạng thái quiz của phòng
    RoomInfo rooms[MAX_ROOMS_DISPLAY];  // Danh sách phòng
    int room_count;                 // Số lượng phòng
    
    // Host panel state
    int quiz_duration;              // Thời gian làm bài (giây)
    char question_file[64];         // File câu hỏi đã chọn
    ParticipantInfo participants[MAX_PARTICIPANTS]; // Danh sách thí sinh
    int participant_count;          // Số thí sinh
    int stats_waiting;              // Số người đang chờ
    int stats_taking;               // Số người đang làm
    int stats_submitted;            // Số người đã nộp
    
    // Quiz state
    Question questions[MAX_QUESTIONS]; // Các câu hỏi
    int question_count;             // Số câu hỏi
    int current_question;           // Câu hỏi hiện tại
    char answers[MAX_QUESTIONS];    // Đáp án của người dùng
    int score;                      // Điểm cuối cùng
    int total_questions;            // Tổng số câu hỏi
    time_t quiz_start_time;         // Thời điểm bắt đầu
    int time_taken;                 // Thời gian làm bài
    
    // Quiz availability
    bool quiz_available;            // Quiz đã sẵn sàng chưa
    
    // Message buffer
    char message_buffer[BUFFER_SIZE]; // Buffer tin nhắn từ server
    bool has_pending_message;       // Có tin nhắn chờ xử lý
    
    // Status message for UI
    char status_message[128];       // Thông báo trạng thái
} ClientContext;
```

### 8.3 Định dạng file câu hỏi (questions.txt)

```
ID;Question;A.opt|B.opt|C.opt|D.opt;Answer
```

**Ví dụ:**
```
1;Thủ đô của Việt Nam là gì?;A.Hà Nội|B.HCM|C.Hải Phòng|D.Đà Nẵng;A
2;Số nguyên tố đầu tiên là?;A.1|B.2|C.3|D.5;B
```

---

## 9. Ràng buộc và giới hạn

### 9.1 Giới hạn hệ thống

| Tham số | Giá trị | Ghi chú |
|---------|---------|---------|
| MAX_CLIENTS | 100 | Số client tối đa |
| MAX_ROOMS | 10 | Số phòng tối đa |
| MAX_QUESTIONS | 20 | Số câu hỏi tối đa/phòng |
| BUFFER_SIZE | 1024 | Kích thước buffer |
| Username length | 31 chars | Độ dài username tối đa |
| Password length | 31 chars | Độ dài password tối đa |

### 9.2 Yêu cầu hệ thống

**Server:**
- Linux OS
- GCC compiler
- Port 8080 available

**Client:**
- Linux OS
- GCC compiler
- Ncurses library
- Terminal với hỗ trợ Unicode

### 9.3 Phụ thuộc thư viện

| Thư viện | Phiên bản | Mục đích |
|----------|-----------|----------|
| ncurses | >= 5.9 | TUI rendering |
| pthread | standard | Threading support |
| POSIX sockets | standard | Network communication |
| epoll | Linux kernel | I/O multiplexing |

---

## Phụ lục A: Phím tắt

| Phím | Chức năng |
|------|-----------|
| TAB | Chuyển focus |
| ENTER | Xác nhận/Chọn |
| ESC | Quay lại/Thoát |
| F10 | Thoát ứng dụng |
| UP/DOWN | Di chuyển menu |
| LEFT/RIGHT | Điều hướng câu hỏi |
| A/B/C/D | Chọn đáp án |
| S | Nộp bài/Bắt đầu game |

---

## Phụ lục B: Mã lỗi

| Code | Message | Mô tả |
|------|---------|-------|
| - | Max rooms reached | Đã đạt giới hạn phòng |
| - | Room is full | Phòng đã đầy |
| - | Room not found | Không tìm thấy phòng |
| - | Not a room host | Không phải host |
| - | Not in any room | Không trong phòng nào |
| - | Failed to load questions | Lỗi đọc câu hỏi |
| - | Quiz not available yet | Quiz chưa được host bắt đầu |
| - | Cannot change after quiz started | Không thể thay đổi cấu hình sau khi quiz bắt đầu |
| - | You already have a room | User đã có phòng, không thể tạo thêm |
| - | No room found for this user | Không tìm thấy phòng để rejoin |

---

## 10. Danh sách chức năng chi tiết

### 10.1 Chức năng phía Server

| STT | Chức năng | Mô tả | Trạng thái |
|-----|-----------|-------|------------|
| 1 | Cài đặt cơ chế I/O qua socket | Sử dụng epoll để xử lý nhiều kết nối đồng thời với non-blocking I/O | ✅ Hoàn thành |
| 2 | Xử lý luồng (Stream handling) | Parse và xử lý các tin nhắn theo format TYPE:DATA | ✅ Hoàn thành |
| 3 | Đăng ký và quản lý tài khoản | Quản lý thông tin client kết nối, lưu username | ⚠️ Cơ bản |
| 4 | Đăng nhập và quản lý phiên làm việc | Theo dõi trạng thái client, hỗ trợ reconnect cho host | ✅ Hoàn thành |
| 5 | Quản lý quyền truy cập | Phân biệt Host và Participant, kiểm tra quyền cho các action | ✅ Hoàn thành |
| 6 | Nộp bài và chấm điểm | Nhận đáp án, so sánh với đáp án đúng, tính điểm và thời gian | ✅ Hoàn thành |
| 7 | Ghi log hoạt động | Ghi log các sự kiện quan trọng ra console | ✅ Hoàn thành |
| 8 | Lưu trữ thông tin bài thi + thống kê | Theo dõi trạng thái và kết quả của từng thí sinh trong phòng | ✅ Hoàn thành |
| 9 | Phân loại câu hỏi (Backend) | Load câu hỏi từ file, hỗ trợ nhiều file câu hỏi | ✅ Hoàn thành |

### 10.2 Chức năng phía Client

| STT | Chức năng | Mô tả | Trạng thái |
|-----|-----------|-------|------------|
| 1 | Giao diện đồ họa người dùng (TUI) | Sử dụng Ncurses để xây dựng giao diện trên terminal | ✅ Hoàn thành |
| 2 | Đăng nhập, kết nối lại | Kết nối server, nhập username, hỗ trợ rejoin phòng đã tạo | ✅ Hoàn thành |
| 3 | Tham gia chế độ luyện tập | *(Chưa triển khai - kế hoạch tương lai)* | ❌ Chưa làm |
| 4 | Tạo phòng thi | Gửi yêu cầu tạo phòng với cấu hình tùy chỉnh | ✅ Hoàn thành |
| 5 | Xem danh sách các phòng thi | Hiển thị danh sách phòng với thông tin host, số người, trạng thái | ✅ Hoàn thành |
| 6 | Tham gia phòng thi | Nhập Room ID để join vào phòng có sẵn | ✅ Hoàn thành |
| 7 | Bắt đầu bài thi (Nhận đề và xử lý) | Nhận câu hỏi từ server, parse và hiển thị | ✅ Hoàn thành |
| 8 | Thay đổi đáp án đã chọn | Cho phép thay đổi đáp án trước khi nộp bài | ✅ Hoàn thành |
| 9 | Xem kết quả bài thi đã hoàn thành | Hiển thị điểm, phần trăm, thời gian và đánh giá | ✅ Hoàn thành |
| 10 | Thống kê bài thi | Host Panel hiển thị thống kê real-time về thí sinh | ✅ Hoàn thành |

### 10.3 Tổng hợp tiến độ

| Thành phần | Hoàn thành | Cơ bản | Chưa làm | Tổng |
|------------|------------|--------|----------|------|
| Server | 8 | 1 | 0 | 9 |
| Client | 9 | 0 | 1 | 10 |
| **Tổng** | **17** | **1** | **1** | **19** |

**Tiến độ tổng thể: ~95%**

---

*Tài liệu này được cập nhật lần cuối vào ngày 02/12/2025*
