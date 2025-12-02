# Báo cáo Tiến độ - Checkpoint 1 (CP1)
# Dự án Quizzie - Hệ thống thi trắc nghiệm trực tuyến

**Ngày báo cáo:** 02/12/2025  
**Trạng thái:** Checkpoint 1 hoàn thành

---

## Mục lục

1. [Tổng quan tiến độ](#1-tổng-quan-tiến-độ)
2. [Chi tiết các module đã hoàn thành](#2-chi-tiết-các-module-đã-hoàn-thành)
3. [Cấu trúc mã nguồn](#3-cấu-trúc-mã-nguồn)
4. [Hướng dẫn build và chạy](#4-hướng-dẫn-build-và-chạy)
5. [Các tính năng đã hoạt động](#5-các-tính-năng-đã-hoạt-động)
6. [Các vấn đề và giới hạn hiện tại](#6-các-vấn-đề-và-giới-hạn-hiện-tại)
7. [Kế hoạch tiếp theo](#7-kế-hoạch-tiếp-theo)

---

## 1. Tổng quan tiến độ

### 1.1 Tóm tắt

| Thành phần | Trạng thái | Tiến độ |
|------------|------------|---------|
| Server Core | ✅ Hoàn thành | 100% |
| Server Network (epoll) | ✅ Hoàn thành | 100% |
| Server Room Management | ✅ Hoàn thành | 100% |
| Server Storage | ✅ Hoàn thành | 100% |
| Client Core | ✅ Hoàn thành | 100% |
| Client Network | ✅ Hoàn thành | 100% |
| Client UI Framework | ✅ Hoàn thành | 100% |
| Client Pages (6 pages) | ✅ Hoàn thành | 100% |
| Giao thức truyền thông | ✅ Hoàn thành | 100% |
| Host Panel & Statistics | ✅ Hoàn thành | 100% |
| Room List & Join | ✅ Hoàn thành | 100% |
| Xác thực người dùng | X |  |
| Bộ câu hỏi mẫu | ✅ Hoàn thành | 100% |

### 1.2 Tiến độ tổng thể: **~95%** cho CP1

---

## 2. Chi tiết các module đã hoàn thành

### 2.1 Server

#### 2.1.1 Core Module (`server/src/core/server.c`)

**Đã triển khai:**
- ✅ `server_init()`: Khởi tạo ServerContext với các giá trị mặc định
- ✅ `server_cleanup()`: Giải phóng tài nguyên (epoll_fd, server_fd, memory)
- ✅ `server_run()`: Event loop chính sử dụng epoll
- ✅ `parse_message()`: Parse tin nhắn theo format TYPE:DATA

**Tính năng:**
```c
// Khởi tạo context
ServerContext* ctx = (ServerContext*)malloc(sizeof(ServerContext));
ctx->server_fd = -1;
ctx->epoll_fd = -1;
ctx->client_count = 0;
ctx->room_count = 0;
ctx->running = true;

// Event loop với epoll_wait
while (ctx->running) {
    int event_count = epoll_wait(ctx->epoll_fd, events, MAX_CLIENTS, -1);
    // Xử lý các events...
}
```

**Các message types được xử lý:**
- `CREATE_ROOM`: Tạo phòng mới với cấu hình
- `JOIN_ROOM`: Tham gia phòng
- `START_GAME`: Host bắt đầu quiz
- `CLIENT_START`: Thí sinh bắt đầu làm bài
- `GET_STATS`: Lấy thống kê phòng
- `SET_CONFIG`: Cập nhật cấu hình phòng
- `LIST_ROOMS`: Lấy danh sách phòng
- `REJOIN_HOST`: Host kết nối lại
- `SUBMIT`: Nộp bài

#### 2.1.2 Network Module (`server/src/net/net.c`)

**Đã triển khai:**
- ✅ `net_setup()`: Thiết lập socket TCP, bind, listen, epoll
- ✅ `net_accept_client()`: Accept client mới và thêm vào epoll
- ✅ `net_send_to_client()`: Gửi dữ liệu đến client
- ✅ `net_receive_from_client()`: Nhận dữ liệu từ client
- ✅ `net_close_client()`: Đóng kết nối và xóa khỏi epoll

**Tính năng I/O:**
- Non-blocking sockets với `fcntl(F_SETFL, O_NONBLOCK)`
- I/O Multiplexing với `epoll_create1()` và `epoll_ctl()`

```c
// Setup non-blocking socket
int flags = fcntl(ctx->server_fd, F_GETFL, 0);
fcntl(ctx->server_fd, F_SETFL, flags | O_NONBLOCK);

// Setup epoll
ctx->epoll_fd = epoll_create1(0);
struct epoll_event event = {.events = EPOLLIN, .data.fd = ctx->server_fd};
epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->server_fd, &event);
```

#### 2.1.3 Room Module (`server/src/room/room.c`)

**Đã triển khai:**
- ✅ `room_create()`: Tạo phòng mới, load câu hỏi, assign host với cấu hình
- ✅ `room_join()`: Thêm client vào phòng đã tồn tại
- ✅ `room_start_quiz()`: Host bắt đầu quiz, thông báo đến tất cả thí sinh
- ✅ `room_client_start_quiz()`: Thí sinh bắt đầu làm bài cá nhân
- ✅ `room_submit_answers()`: Nhận và chấm điểm đáp án, tính thời gian
- ✅ `room_get_stats()`: Lấy thống kê real-time cho host
- ✅ `room_list()`: Liệt kê tất cả phòng
- ✅ `room_rejoin_as_host()`: Host kết nối lại phòng đã tạo
- ✅ `room_set_config()`: Cập nhật cấu hình phòng (thời gian, file câu hỏi)
- ✅ `room_find_by_host_username()`: Tìm phòng theo username host

**Logic chấm điểm với thời gian:**
```c
// So sánh từng đáp án với đáp án đúng
char* token = strtok(correct_copy, ",");
while (token && idx < strlen(submitted)) {
    if (submitted[idx] == token[0]) {
        score++;
    }
    idx++;
    token = strtok(NULL, ",");
}

// Tính thời gian làm bài
time_t now = time(NULL);
int time_taken = client->quiz_start_time > 0 ? (int)(now - client->quiz_start_time) : 0;

// Gửi kết quả: "RESULT:score/total,time_taken"
```

**Thống kê phòng real-time:**
```c
// Format: ROOM_STATS:waiting,taking,submitted;user1:status:value,...
// status: W (waiting), T (taking), S (submitted)
// value: remaining_time (nếu T), score/total (nếu S)
```

#### 2.1.4 Storage Module (`server/src/storage/storage.c`)

**Đã triển khai:**
- ✅ `storage_load_questions()`: Đọc và parse file câu hỏi
- ✅ `storage_save_log()`: Ghi log vào file

**Format câu hỏi:**
```
ID;Question;A.opt|B.opt|C.opt|D.opt;Answer
```

**Ví dụ file `data/questions.txt`:**
```
1;Thủ đô của Việt Nam là gì?;A.Hà Nội|B.HCM|C.Hải Phòng|D.Đà Nẵng;A
2;Số nguyên tố đầu tiên là?;A.1|B.2|C.3|D.5;B
3;C là ngôn ngữ gì?;A.Hướng đối tượng|B.Biên dịch|C.Thông dịch|D.Scripting;B
4;TCP hoạt động ở tầng nào?;A.Vận chuyển|B.Mạng|C.Vật lý|D.Ứng dụng;A
5;Tác giả truyện Kiều?;A.Nguyễn Du|B.Nguyễn Trãi|C.Nam Cao|D.Hồ Xuân Hương;A
```

---

### 2.2 Client

#### 2.2.1 Core Module (`client/src/core/client.c`)

**Đã triển khai:**
- ✅ `client_init()`: Khởi tạo ClientContext với đầy đủ trạng thái
- ✅ `client_cleanup()`: Giải phóng tài nguyên
- ✅ `client_run()`: Main loop với state machine 6 trạng thái
- ✅ `client_send_message()`: Gửi tin nhắn theo format TYPE:DATA
- ✅ `client_receive_message()`: Nhận tin non-blocking
- ✅ `client_process_server_message()`: Parse và xử lý tất cả response types
- ✅ `parse_questions()`: Parse câu hỏi với format mới (có duration)
- ✅ `parse_room_list()`: Parse danh sách phòng
- ✅ `parse_room_stats()`: Parse thống kê phòng cho host

**State Machine (6 states):**
```c
typedef enum {
    PAGE_LOGIN,       // Màn hình đăng nhập
    PAGE_DASHBOARD,   // Menu chính
    PAGE_ROOM_LIST,   // Danh sách phòng
    PAGE_QUIZ,        // Làm bài thi
    PAGE_RESULT,      // Kết quả
    PAGE_HOST_PANEL   // Panel điều khiển của Host (MỚI)
} AppState;
```

**Các message types được xử lý:**
- `ROOM_CREATED`: Nhận room ID và chuyển đến Host Panel
- `ROOM_REJOINED`: Khôi phục host session
- `JOINED`: Vào phòng thành công
- `ROOM_LIST`: Parse và hiển thị danh sách phòng
- `QUIZ_AVAILABLE`: Thông báo quiz sẵn sàng
- `QUIZ_STARTED`: Xác nhận cho host
- `QUESTIONS`: Nhận và parse câu hỏi
- `ROOM_STATS`: Cập nhật thống kê
- `CONFIG_UPDATED`: Xác nhận cấu hình
- `PARTICIPANT_JOINED/STARTED/SUBMITTED`: Thông báo trạng thái thí sinh
- `RESULT`: Nhận kết quả với thời gian
- `ERROR`: Hiển thị lỗi

**Main Loop:**
```c
while (ctx->running) {
    if (ctx->connected) client_receive_message(ctx);
    
    erase();
    switch (ctx->current_state) {
        case PAGE_LOGIN: page_login_draw(ctx); break;
        case PAGE_DASHBOARD: page_dashboard_draw(ctx); break;
        // ...
    }
    refresh();
    
    timeout(100);
    int ch = ui_get_input();
    // Handle input...
}
```

#### 2.2.2 Network Module (`client/src/net/net.c`)

**Đã triển khai:**
- ✅ `net_connect()`: Kết nối TCP đến server
- ✅ `net_send()`: Gửi dữ liệu
- ✅ `net_receive()`: Nhận dữ liệu (blocking)
- ✅ `net_receive_nonblocking()`: Nhận với timeout (sử dụng select)
- ✅ `net_close()`: Đóng kết nối

**Non-blocking receive:**
```c
int net_receive_nonblocking(ClientContext* ctx, char* buffer, size_t len, int timeout_ms) {
    fd_set read_fds;
    FD_SET(ctx->socket_fd, &read_fds);
    
    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };
    
    int result = select(ctx->socket_fd + 1, &read_fds, NULL, NULL, &tv);
    if (result > 0 && FD_ISSET(ctx->socket_fd, &read_fds)) {
        return recv(ctx->socket_fd, buffer, len - 1, 0);
    }
    return 0; // Timeout
}
```

#### 2.2.3 UI Module (`client/src/ui/`)

**Đã triển khai:**

| File | Chức năng |
|------|-----------|
| `ui.c` | Core UI: init, cleanup, get_input |
| `pages/login.c` | Màn hình đăng nhập |
| `pages/dashboard.c` | Menu chính |
| `pages/room_list.c` | Danh sách phòng + join/rejoin |
| `pages/host_panel.c` | **MỚI**: Panel điều khiển host với thống kê real-time |
| `pages/quiz.c` | Làm bài thi |
| `pages/result.c` | Hiển thị kết quả với thời gian |

**Mỗi page có 2 hàm chính:**
- `page_xxx_draw(ctx)`: Vẽ giao diện
- `page_xxx_handle_input(ctx, input)`: Xử lý input

**Host Panel features:**
- Xem thông tin phòng và trạng thái
- Cấu hình thời gian và file câu hỏi
- Bắt đầu quiz
- Thống kê real-time: số người đang chờ/làm/nộp
- Danh sách thí sinh với trạng thái chi tiết

**Ncurses features sử dụng:**
```c
initscr();           // Khởi tạo
cbreak();            // Tắt line buffering
noecho();            // Không echo input
keypad(stdscr, TRUE); // Hỗ trợ phím đặc biệt
mousemask(ALL_MOUSE_EVENTS, NULL); // Hỗ trợ chuột
curs_set(0);         // Ẩn cursor

attron(A_BOLD);      // Bold text
attron(A_REVERSE);   // Highlight
mvprintw(y, x, "text"); // In tại vị trí
```

---

## 3. Cấu trúc mã nguồn

```
quizzie/
├── data/
│   └── questions.txt        # Bộ câu hỏi mẫu (5 câu)
├── docs/
│   ├── SPEC.md              # Tài liệu SRS (v2.0)
│   └── CP1.md               # Báo cáo này
├── server/
│   ├── Makefile             # Build script
│   ├── bin/
│   │   └── server           # Binary đã build
│   ├── include/
│   │   ├── common.h         # Constants, macros
│   │   ├── server.h         # Server structs & prototypes (Client, Room, QuizState)
│   │   ├── net.h            # Network prototypes
│   │   ├── room.h           # Room management prototypes (10 functions)
│   │   └── storage.h        # Storage prototypes
│   └── src/
│       ├── main.c           # Entry point
│       ├── core/server.c    # Server logic + message routing
│       ├── net/net.c        # Network (epoll)
│       ├── room/room.c      # Room management (create, join, stats, config, etc.)
│       └── storage/storage.c # File I/O
└── client/
    ├── Makefile             # Build script
    ├── bin/
    │   └── client           # Binary đã build
    ├── include/
    │   ├── common.h         # Constants, macros
    │   ├── client.h         # Client structs & prototypes (Question, RoomInfo, ParticipantInfo, ClientContext)
    │   ├── net.h            # Network prototypes
    │   └── ui.h             # UI prototypes (6 pages)
    └── src/
        ├── main.c           # Entry point
        ├── core/client.c    # Client logic, state machine, message processing
        ├── net/net.c        # Network (select)
        └── ui/
            ├── ui.c         # Ncurses init/cleanup
            └── pages/
                ├── login.c       # Login page
                ├── dashboard.c   # Dashboard page
                ├── room_list.c   # Room list + join/rejoin
                ├── host_panel.c  # Host control panel (NEW)
                ├── quiz.c        # Quiz page
                └── result.c      # Result page with time
```

---

## 4. Hướng dẫn build và chạy

### 4.1 Yêu cầu

```bash
# Ubuntu/Debian
sudo apt-get install build-essential libncurses5-dev

# Fedora
sudo dnf install gcc ncurses-devel
```

### 4.2 Build

```bash
# Build server
cd server
make clean && make

# Build client
cd ../client
make clean && make
```

### 4.3 Chạy

```bash
# Terminal 1: Start server
cd server
./bin/server

# Terminal 2+: Start client(s)
cd client
./bin/client
```

### 4.4 Luồng sử dụng

**Cho Host (Người tạo phòng):**
1. **Đăng nhập:** Nhập username → ENTER
2. **Dashboard:** Chọn "Create Room"
3. **Host Panel:** 
- Cấu hình thời gian (D) và file câu hỏi (F)
- Chờ thí sinh tham gia
- Nhấn 'S' để bắt đầu quiz
- Xem thống kê real-time (R để refresh)
4. **Kết thúc:** ESC để rời phòng

**Cho Participant (Thí sinh):**
1. **Đăng nhập:** Nhập username → ENTER
2. **Dashboard:** Chọn "View Rooms"
3. **Room List:** 
- Xem danh sách phòng
- Nhập Room ID để tham gia
4. **Quiz:** 
- Chờ host bắt đầu (hoặc nhấn 'S' nếu quiz đã sẵn sàng)
- Nhận câu hỏi và làm bài
- Chọn A/B/C/D, LEFT/RIGHT để chuyển câu
- Nhấn 'S' để nộp bài
5. **Result:** Xem điểm, thời gian, chọn Play Again hoặc Dashboard

**Kết nối lại (Reconnect):**
1. Host đăng nhập lại với cùng username
2. Chọn "View Rooms"
3. Phòng của host sẽ hiển thị với dấu "[My Room]"
4. Chọn "Rejoin" để quay lại Host Panel

---

## 5. Các tính năng đã hoạt động

### 5.1 Server

| Tính năng | Trạng thái | Ghi chú |
|-----------|------------|---------|
| Multi-client support | ✅ | Sử dụng epoll |
| Non-blocking I/O | ✅ | fcntl O_NONBLOCK |
| Room creation với cấu hình | ✅ | Thời gian + file câu hỏi |
| Room joining | ✅ | Validate room ID, track username |
| Quiz broadcast | ✅ | Thông báo QUIZ_AVAILABLE |
| Individual quiz start | ✅ | Mỗi thí sinh bắt đầu riêng |
| Answer scoring với thời gian | ✅ | Tính điểm + time_taken |
| Room statistics | ✅ | Real-time stats cho host |
| Host reconnect | ✅ | Dựa trên username |
| Room configuration | ✅ | Duration + question file |
| Room listing | ✅ | Liệt kê tất cả phòng |
| Question loading | ✅ | Từ file text, multi-path support |
| Error handling | ✅ | Log và gửi error message |

### 5.2 Client

| Tính năng | Trạng thái | Ghi chú |
|-----------|------------|---------|
| TUI với Ncurses | ✅ | 6 pages hoàn chỉnh |
| Non-blocking receive | ✅ | Sử dụng select() |
| State machine | ✅ | 6 states bao gồm Host Panel |
| Keyboard navigation | ✅ | TAB, Arrows, A-D, ESC, S/R/D/F/Q |
| Mouse support | ✅ | Click vào sidebar |
| Room list display | ✅ | Hiển thị với host, player count, state |
| Host Panel | ✅ | Cấu hình + thống kê real-time |
| Quiz waiting | ✅ | Chờ host bắt đầu |
| Individual quiz start | ✅ | Nhấn S khi sẵn sàng |
| Question display | ✅ | Word wrap, highlight |
| Answer selection/change | ✅ | Visual feedback |
| Score display với thời gian | ✅ | Điểm + % + thời gian + đánh giá |
| Host reconnect | ✅ | Rejoin phòng đã tạo |

---

## 6. Các vấn đề và giới hạn hiện tại

### 6.1 Giới hạn đã biết

| Vấn đề | Mức độ | Giải pháp tương lai |
|--------|--------|---------------------|
| Password không được xác thực | Thấp | Thêm hệ thống auth với database |
| Không có persistent data | Trung bình | Thêm database/file lưu trữ |
| Không có chế độ luyện tập | Trung bình | Thêm Practice mode |
| Không có leaderboard toàn cục | Thấp | Thêm tính năng |
| UI không có màu | Thấp | Enable color pairs |
| Không có timer hiển thị | Trung bình | Thêm countdown timer |

### 6.2 Các điểm cần cải thiện

1. **Xác thực:** Hiện tại password không được validate, chỉ cần username
2. **Persistence:** Không lưu lại lịch sử thi, điểm số lâu dài
3. **Auto-reconnect:** Thí sinh không tự động reconnect khi mất kết nối
4. **Timer:** Chưa hiển thị countdown timer cho thí sinh
5. **Unicode:** Một số ký tự tiếng Việt có thể hiển thị không đúng trên một số terminal

### 6.3 Các tính năng mới đã thêm so với kế hoạch ban đầu

1. ✅ **Host Panel:** Giao diện quản lý riêng cho host
2. ✅ **Room Configuration:** Cấu hình thời gian và file câu hỏi
3. ✅ **Real-time Statistics:** Thống kê trạng thái thí sinh
4. ✅ **Room List:** Xem danh sách tất cả phòng
5. ✅ **Host Reconnect:** Cho phép host kết nối lại
6. ✅ **Individual Quiz Start:** Mỗi thí sinh tự bắt đầu khi sẵn sàng
7. ✅ **Time Tracking:** Theo dõi thời gian làm bài của từng thí sinh

---

## 7. Kế hoạch tiếp theo

### 7.1 Checkpoint 2 (CP2)

| Tính năng | Độ ưu tiên | Ước lượng |
|-----------|------------|-----------|
| Xác thực với file users.txt | Cao | 2h |
| Lưu lịch sử điểm | Cao | 3h |
| Hiển thị countdown timer | Cao | 2h |
| Practice mode (luyện tập cá nhân) | Cao | 3h |
| Leaderboard trong phòng | Trung bình | 2h |
| UI màu sắc | Thấp | 1h |

### 7.2 Checkpoint 3 (CP3)

| Tính năng | Độ ưu tiên | Ước lượng |
|-----------|------------|-----------|
| Realtime leaderboard | Cao | 4h |
| Chat trong phòng | Trung bình | 3h |
| Auto-timeout khi hết giờ | Cao | 2h |
| Kick/Ban player | Thấp | 2h |
| Admin dashboard | Thấp | 4h |
| Nhiều loại câu hỏi (điền đáp án, ...) | Trung bình | 4h |

---

## Phụ lục: Screenshots

### Login Page
```
                    QUIZZIE - Login
                
                [Connected to server]

                Username: [testuser     ]
                Password: [              ]

                        [ LOGIN ]

TAB: Switch focus | ENTER: Select | F10: Quit
```

### Host Panel (MỚI)
```
┌──────────────┬─────────────────────────────────────┐
│ HOST PANEL   │ New participant: user2              │
│              │                                     │
│ Room ID: 1   │ PARTICIPANTS STATISTICS             │
│ Host: admin  │ ─────────────────────────────────── │
│              │ Waiting: 1 | Taking: 1 | Submitted: 1
│ Status:      │ ─────────────────────────────────── │
│   Waiting    │ Username     Status      Info       │
│              │ ─────────────────────────────────── │
│ Configuration│ user1        Waiting               │
│ Duration: 300│ user2        Taking      04:32 left│
│ File: ques.. │ user3        Submitted   4/5       │
│              │                                     │
│ Actions:     │                                     │
│ [S] Start    │                                     │
│ [D] Duration │                                     │
│ [F] Set File │                                     │
│ [Q] Leave    │                                     │
├──────────────┴─────────────────────────────────────┤
│ UP/DOWN: Select | ENTER: Action | ESC: Leave       │
└────────────────────────────────────────────────────┘
```

### Quiz Page
```
┌──────────────┬─────────────────────────────────────┐
│ QUIZ         │ Question 1:                         │
│              │                                     │
│ Room: 1      │ Thủ đô của Việt Nam là gì?         │
│ Player: test │                                     │
│              │ [A] A.Hà Nội                       │
│ Progress:    │ [B] B.HCM                          │
│ 1 / 5        │ [C] C.Hải Phòng                    │
│              │ [D] D.Đà Nẵng                      │
│ Questions:   │                                     │
│   Q1 [A]     │ Selected: A                         │
│   Q2 [-]     │                                     │
│   Q3 [-]     │ Press S when ready to start!       │
│   Q4 [-]     │                                     │
│   Q5 [-]     │                                     │
├──────────────┴─────────────────────────────────────┤
│ A-D: Answer | LEFT/RIGHT: Nav | S: Submit | ESC   │
└────────────────────────────────────────────────────┘
```

### Result Page (với thời gian)
```
┌────────────────────────────────────────────────────┐
│                                                    │
│                   QUIZ RESULTS                     │
│                                                    │
│                    Your Score:                     │
│                      4 / 5                         │
│                                                    │
│                 Percentage: 80.0%                  │
│                                                    │
│               Time taken: 02:45                    │
│                                                    │
│              Excellent! Great job!                 │
│                                                    │
│                 [ Play Again ]                     │
│                 [ Dashboard  ]                     │
│                                                    │
│ UP/DOWN: Select | ENTER: Confirm | F10: Quit      │
└────────────────────────────────────────────────────┘
```

---

*Báo cáo được cập nhật vào ngày 02/12/2025*
