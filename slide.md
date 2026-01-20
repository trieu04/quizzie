# Quizzie - Hệ thống Thi Trắc Nghiệm Trực Tuyến

## TCP Connection State Machine

Hệ thống sử dụng cơ chế kết nối TCP bền vững (Persistent Connection) kết hợp với Header nhị phân và Payload JSON. Dưới đây là mô hình trạng thái kết nối ở hai phía:

### 1. Phía Server (State Machine)

Server quản lý client thông qua socket và định danh người dùng sau khi đăng nhập thành công:

```mermaid
stateDiagram-v2
    [*] --> LISTENING
    LISTENING --> TCP_ESTABLISHED : accept()

    state TCP_ESTABLISHED {
        [*] --> GUEST : Waiting for Login
        GUEST --> CONNECTED : RES (LOGIN SUCCESS)

        state CONNECTED {
            [*] --> WAIT_REQ
            WAIT_REQ --> PROCESSING : Receive REQ
            PROCESSING --> WAIT_REQ : Send RES / UPD
        }

        CONNECTED --> GUEST : REQ (LOGOUT)
    }

    TCP_ESTABLISHED --> [*] : Connection Lost / close()
```

---

### 2. Phía Client (State Machine)

Ở phía Client, trạng thái `CONNECTED` đại diện cho việc phiên làm việc đã được thiết lập thành công:

```mermaid
stateDiagram-v2
    [*] --> DISCONNECTED
    DISCONNECTED --> AUTHENTICATING : Login Triggered
    AUTHENTICATING --> CONNECTED : RES (LOGIN SUCCESS)
    AUTHENTICATING --> DISCONNECTED : RES (LOGIN FAIL) / Error

    state CONNECTED {
        [*] --> IDLE : View Current Screen
        IDLE --> SENDING : User Action / Timer Triggered
        SENDING --> WAITING : Send REQ / HBT
        WAITING --> IDLE : Receive RES / UPD / HBT_ACK

        state IDLE {
            [*] --> DASHBOARD
            DASHBOARD --> ROOM_VIEW
            ROOM_VIEW --> EXAM_MODE
        }

        --
        [*] --> HEARTBEAT_TIMER
        HEARTBEAT_TIMER --> SENDING : Timeout (30s)
    }

    CONNECTED --> DISCONNECTED : REQ (LOGOUT) / Socket Error
```

**Đặc điểm Client:**
- **ensure_connection()**: Hàm này đảm bảo socket luôn sẵn sàng trước khi gửi dữ liệu. Nếu mất kết nối, nó sẽ thử tạo lại socket.
- **DASHBOARD_VIEW**: Màn hình chính sau khi đăng nhập, hiển thị danh sách các phòng thi khả dụng.
- **EXAM_VIEW**: Màn hình làm bài thi với đồng hồ đếm ngược và cơ chế gửi đáp án tức thời (Immediate Feedback).

---

### 3. Cơ chế Heartbeat & Xử lý Ngoại lệ

Để đảm bảo tính thời gian thực và độ tin cậy, hệ thống triển khai cơ chế Heartbeat (HBT) và các kịch bản xử lý lỗi kết nối.

#### 3.1. Luồng Heartbeat (Keep-alive)
Cơ chế này giúp Server nhận biết Client còn hoạt động hay không và ngược lại, tránh việc giữ các tài nguyên "treo".

```mermaid
sequenceDiagram
    participant Client
    participant Server

    Note over Client, Server: Trạng thái: CONNECTED

    loop Mỗi 5 giây
        Client->>Server: REQ (MSG_TYPE: HBT)
        alt Server Alive
            Server-->>Client: RES (MSG_TYPE: HBT)
        else Server Down / Network Error
            Note right of Client: Timeout / Connection Error
            Client->>Client: Chuyển sang DISCONNECTED
        end
    end
```

#### 3.2. Xử lý Ngắt kết nối & Rejoin
Khi một kết nối bị ngắt đột ngột (mất mạng, crash), hệ thống xử lý như sau:

**Phía Server:**
1. **Phát hiện**: `poll()` trả về sự kiện lỗi hoặc `recv()` trả về 0.
2. **Xử lý**:
   - Đóng socket, gọi `client_remove()` để giải phóng tài nguyên.
   - **Quan trọng**: Nếu client đang trong `TESTING`, Server KHÔNG xóa phiên thi (`ExamSession`) mà giữ lại trong bộ nhớ tạm/file.
3. **Timeout**: Nếu sau một khoảng thời gian (ví dụ 5 phút) client không quay lại, phiên thi mới chính thức bị hủy.

**Phía Client:**
1. **Phát hiện**: Gửi/Nhận tin nhắn thất bại.
2. **Xử lý**:
   - Hiển thị thông báo mất kết nối.
   - Gọi `ensure_connection()` để thử tạo lại socket TCP.
   - Gửi yêu cầu `LOGIN` lại.
   - Sau khi login, nếu có phiên thi cũ, Server sẽ gửi lại trạng thái bài thi (`UPD`) để Client khôi phục `EXAM_MODE`.

#### 3.3. Sơ đồ Xử lý Ngoại lệ (Exception Handling)

```mermaid
stateDiagram-v2
    [*] --> NORMAL_OPERATION
    NORMAL_OPERATION --> ERROR_DETECTED : Socket Error / Timeout

    state ERROR_DETECTED {
        [*] --> CLEANUP_SERVER : Server Side
        [*] --> RECONNECT_CLIENT : Client Side

        CLEANUP_SERVER --> SAVE_SESSION : If in Exam
        SAVE_SESSION --> [*]

        RECONNECT_CLIENT --> RETRY_CONNECTION : ensure_connection()
        RETRY_CONNECTION --> RE_AUTH : Connection Success
        RE_AUTH --> RESTORE_STATE : Login Success
        RESTORE_STATE --> NORMAL_OPERATION : Back to Exam/Dashboard
    }

    ERROR_DETECTED --> DISCONNECTED : Max Retries Exceeded
    DISCONNECTED --> [*]
```

---

### 4. Các Luồng Nghiệp vụ Chính (Sequence Diagrams)

Dưới đây là các luồng tương tác chính giữa Client và Server dựa trên đặc tả SRS.

#### 4.1. Đăng nhập (Authentication)
```mermaid
sequenceDiagram
    participant C as Client
    participant S as Server
    participant DB as Storage (Files)

    C->>S: REQ (LOGIN, {user, pass})
    S->>DB: Verify credentials
    DB-->>S: Match Found
    S->>S: client_kick_duplicate_user()
    S-->>C: RES (LOGIN SUCCESS, {role})
```

#### 4.2. Quản lý Phòng thi (Admin Flow)
```mermaid
sequenceDiagram
    participant A as Client (Admin)
    participant S as Server
    participant DB as Storage

    A->>S: REQ (CREATE_ROOM, {name, time, questions...})
    S->>S: client_check_admin()
    S->>DB: Save to rooms.json
    S-->>A: RES (CREATE_ROOM SUCCESS)

    loop Mỗi 5 giây
        S->>S: check_room_expiration()
        Note right of S: Update OPEN -> CLOSED if expired
    end
```

#### 4.3. Luồng Thi & Nộp bài (Participant Flow)
```mermaid
sequenceDiagram
    participant P as Participant Client
    participant S as Server
    participant DB as Storage

    P->>S: REQ (GET_ROOM_LIST)
    S-->>P: RES (ROOM_LIST)

    P->>S: REQ (JOIN_ROOM, {room_id})
    S->>DB: Check room status & attempts
    DB-->>S: OK
    S->>S: Shuffle questions (No correct answers)
    S-->>P: RES (JOIN_ROOM SUCCESS, {questions, duration})

    loop Trong lúc làm bài
        P->>S: REQ (SUBMIT_ANSWER, {question_id, choice})
        S->>DB: Update ExamSession (sessions/)
        S-->>P: RES (SUBMIT_ANSWER SUCCESS)
    end

    P->>S: REQ (FINISH_EXAM)
    S->>S: Calculate score
    S->>DB: Save result (results/) & Clear session
    S-->>P: RES (FINISH_EXAM SUCCESS, {score, total})
```

#### 4.4. Khôi phục phiên thi (Rejoin Persistence)
```mermaid
sequenceDiagram
    participant P as Participant Client
    participant S as Server
    participant DB as Storage

    Note over P, S: Giả sử Client bị ngắt kết nối khi đang thi

    P->>S: REQ (LOGIN, {user, pass})
    S-->>P: RES (LOGIN SUCCESS)

    P->>S: REQ (JOIN_ROOM, {room_id})
    S->>DB: Check for existing session in sessions/
    DB-->>S: Session Found
    S-->>P: RES (REJOIN SUCCESS, {questions, answers_done, time_left})
    Note over P: Khôi phục giao diện EXAM_MODE
```

#### 4.5. Quản lý Câu hỏi (Question CRUD)
Hệ thống cho phép Admin quản lý toàn diện các ngân hàng câu hỏi thông qua giao diện biên tập trực quan.

```mermaid
sequenceDiagram
    participant A as Client (Admin)
    participant S as Server
    participant DB as Storage

    Note over A, S: 1. Import (CSV to JSON)
    A->>A: Parse local CSV file
    A->>S: REQ (IMPORT_QUESTIONS, {bank_name, questions})
    S->>DB: Save as [bank_id].json
    S-->>A: RES (IMPORT_SUCCESS)

    Note over A, S: 2. View & Edit
    A->>S: REQ (GET_QUESTION_BANK, {bank_id})
    S->>DB: Read JSON file
    S-->>A: RES (BANK_DETAILS, {questions})
    A->>A: Open Editor (GtkListStore)

    Note over A: Admin modifies questions locally

    A->>S: REQ (UPDATE_QUESTION_BANK, {bank_id, new_questions})
    S->>DB: Overwrite JSON file
    S-->>A: RES (UPDATE_SUCCESS)

    Note over A, S: 3. Delete
    A->>S: REQ (DELETE_QUESTION_BANK, {bank_id})
    S->>DB: Unlink [bank_id].json
    S-->>A: RES (DELETE_SUCCESS)
```

**Đặc điểm thiết kế:**
- **Client-side Parsing**: Server không xử lý file CSV, Client chịu trách nhiệm chuyển đổi sang JSON để giảm tải cho Server.
- **Full State Update**: Khi chỉnh sửa, Client gửi toàn bộ mảng câu hỏi mới để ghi đè, giúp logic đồng bộ đơn giản và tin cậy.
- **Permission**: Mọi yêu cầu đều được kiểm tra `client_check_admin()` trước khi thực thi.

---

### 5. Giao thức Giao tiếp (Packet Structure)

Mọi trạng thái chuyển đổi đều dựa trên việc trao đổi các gói tin theo định dạng:

| Trường | Kích thước | Mô tả |
| :--- | :--- | :--- |
| **TotalLength** | 4 Bytes | `uint32_t` (Big-endian), tổng độ dài gói |
| **MSG_TYPE** | 3 Bytes | `REQ`, `RES`, `ERR`, `UPD`, `HBT` |
| **Payload** | N Bytes | Dữ liệu JSON (Thông tin đăng nhập, câu hỏi, đáp án...) |

---

### 6. Luồng Tương Tác Người Dùng (User Interaction Flow)

Dựa trên cấu trúc UI của Client (GTK-based), hệ thống có các màn hình và luồng tương tác sau:

#### 6.1. Tổng quan các Màn hình UI

| Màn hình | File nguồn | Mô tả |
| :--- | :--- | :--- |
| **Login Screen** | `ui_login.c` | Màn hình đăng nhập/đăng ký, kết nối server |
| **Home Screen** | `ui_home.c` | Danh sách phòng thi cho Participant |
| **Admin Dashboard** | `ui_admin.c` | Quản lý phòng thi và ngân hàng câu hỏi |
| **Room Detail** | `ui_room_detail.c` | Chi tiết phòng thi và lịch sử làm bài |
| **Exam Screen** | `ui_exam.c` | Giao diện làm bài thi với đồng hồ đếm ngược |

---

#### 6.2. Chi tiết các Màn hình

##### 6.2.1. Login Screen (Màn hình Đăng nhập)

**Thành phần giao diện:**
- **Trường nhập liệu:**
  - IP Address: Địa chỉ IP của server
  - Port: Cổng kết nối
  - Username: Tên đăng nhập
  - Password: Mật khẩu (ẩn ký tự)
- **Nút bấm:**
  - "Đăng nhập": Xác thực tài khoản
  - "Đăng ký": Tạo tài khoản mới
- **Nhãn trạng thái:** Hiển thị trạng thái kết nối (Connected/Disconnected)

**Bố cục:** Sử dụng `GtkGrid` căn giữa màn hình, các thành phần xếp theo hàng.

---

##### 6.2.2. Home Screen (Màn hình chính - Thí sinh)

**Thành phần giao diện:**
- **Header:**
  - Nhãn chào mừng với tên người dùng
  - Nút "Đăng xuất"
- **Bảng danh sách phòng thi:** (`GtkTreeView`)
  - Cột: Mã phòng | Tên phòng | Trạng thái | Thời gian mở | Thời lượng
- **Nút bấm:**
  - "Làm mới": Cập nhật danh sách phòng
  - "Xem chi tiết": Mở màn hình Room Detail
- **Nhãn trạng thái:** Hiển thị trạng thái kết nối

**Bố cục:** `GtkBox` dọc - Header → Bảng phòng thi (có scroll) → Nút hành động → Trạng thái.

---

##### 6.2.3. Admin Dashboard (Bảng điều khiển Quản trị)

**Thành phần giao diện:**
- **Header:**
  - Nhãn chào mừng Admin
  - Nút "Đăng xuất"
- **Thanh công cụ:**
  - "Tạo phòng mới": Mở dialog tạo phòng
  - "Quản lý đề thi": Mở trình quản lý ngân hàng câu hỏi
  - "Làm mới": Cập nhật danh sách
  - "Chi tiết": Xem thống kê phòng thi
- **Bảng danh sách phòng:** (`GtkTreeView`)
  - Cột: ID | Tên | Trạng thái | Thời gian mở/đóng | Số câu hỏi | Số lượt thi

**Chức năng chính:**
- Quản lý phòng thi: Tạo, đóng, xóa phòng
- Quản lý ngân hàng câu hỏi: Import CSV, chỉnh sửa, xóa
- Xem thống kê: Kết quả thi, điểm trung bình

---

##### 6.2.4. Room Detail Screen (Màn hình Chi tiết Phòng thi)

**Thành phần giao diện:**
- **Thông tin phòng:** (`GtkGrid`)
  - Mã phòng, Tên phòng, Trạng thái
  - Thời gian bắt đầu/kết thúc
  - Số câu hỏi, Số lượt thi cho phép, Thời lượng
- **Bảng lịch sử làm bài:** (`GtkTreeView`)
  - Cột: Ngày giờ | Số câu | Đã nộp | Số câu đúng
- **Nút bấm:**
  - "Quay lại": Về Home Screen
  - "Bắt đầu" / "Tiếp tục làm": Vào phòng thi
  - "Xem lại bài đã chọn": Xem đáp án đã chọn (nếu được phép)

**Bố cục:** `GtkBox` dọc - Tiêu đề → Grid thông tin → Separator → Lịch sử → Nút hành động.

---

##### 6.2.5. Exam Screen (Màn hình Làm bài thi)

**Thành phần giao diện:**
- **Thanh trên:**
  - Đồng hồ đếm ngược (định dạng MM:SS)
  - Hướng dẫn làm bài
- **Vùng câu hỏi:** (Scrollable container)
  - Các block câu hỏi được tạo động
  - Mỗi câu gồm: Nội dung câu hỏi + 4 `GtkRadioButton` (A, B, C, D)
- **Thanh dưới:**
  - "Thoát": Lưu tiến độ và thoát (có thể quay lại)
  - "Nộp bài": Kết thúc bài thi

**Chức năng đặc biệt:**
- **Lưu tự động:** Mỗi khi chọn đáp án, tự động gửi lên server
- **Tự động nộp:** Khi hết giờ, hệ thống tự động nộp bài
- **Phân biệt Thoát/Nộp:** "Thoát" cho phép quay lại làm tiếp, "Nộp bài" kết thúc hoàn toàn

---

#### 6.3. Luồng Chính - Phân nhánh theo Role

```mermaid
flowchart TD
    A[Khởi động App] --> B[Login Screen]

    B --> C{Nhập thông tin}
    C --> D[IP:Port Server]
    C --> E[Username/Password]

    D --> F{Kết nối Server}
    F -->|Thất bại| G[Hiển thị lỗi]
    G --> B

    F -->|Thành công| H{Đăng nhập/Đăng ký}

    H -->|Đăng ký| I[Gửi REGISTER]
    I -->|Thành công| J[Thông báo thành công]
    J --> B
    I -->|Thất bại| K[User đã tồn tại]
    K --> B

    H -->|Đăng nhập| L[Gửi LOGIN]
    L -->|Thất bại| M[Sai thông tin]
    M --> B

    L -->|Thành công| N{Kiểm tra Role}
    N -->|role = admin| O[Admin Dashboard]
    N -->|role = participant| P[Home Screen]

    style A fill:#4CAF50,color:#fff
    style B fill:#2196F3,color:#fff
    style G fill:#f44336,color:#fff
    style K fill:#f44336,color:#fff
    style M fill:#f44336,color:#fff
    style J fill:#4CAF50,color:#fff
    style O fill:#9C27B0,color:#fff
    style P fill:#FF9800,color:#fff
```

#### 6.4. Luồng Participant (Thí sinh)

```mermaid
flowchart TD
    subgraph HOME ["Home Screen"]
        A1[Danh sách phòng thi] --> A2{Chọn hành động}
        A2 --> A3[Refresh danh sách]
        A2 --> A4[Xem chi tiết phòng]
        A2 --> A5[Đăng xuất]
    end

    A3 --> A1
    A5 --> B1[Login Screen]

    A4 --> C1

    subgraph ROOM ["Room Detail Screen"]
        C1[Thông tin phòng thi] --> C2[Lịch sử làm bài]
        C2 --> C3{Còn lượt thi?}
        C3 -->|Có| C4[Bắt đầu thi]
        C3 -->|Không| C5[Hết lượt]
        C6[Quay lại] --> A1
    end

    C4 --> D1

    subgraph EXAM ["Exam Screen"]
        D1[Bắt đầu đếm ngược] --> D2[Hiển thị câu hỏi]
        D2 --> D3{Chọn đáp án}
        D3 --> D4[Gửi SUBMIT_ANSWER]
        D4 --> D5{Còn câu hỏi?}
        D5 -->|Có| D6[Câu tiếp theo]
        D6 --> D2
        D5 -->|Không| D7[Nộp bài]

        D8[Hết giờ] --> D7
        D7 --> D9[Hiển thị điểm]
    end

    D9 --> A1

    style HOME fill:#E3F2FD,stroke:#1976D2
    style ROOM fill:#FFF3E0,stroke:#F57C00
    style EXAM fill:#E8F5E9,stroke:#388E3C
    style C4 fill:#4CAF50,color:#fff
    style C5 fill:#f44336,color:#fff
    style D7 fill:#2196F3,color:#fff
    style D9 fill:#9C27B0,color:#fff
    style B1 fill:#607D8B,color:#fff
```

#### 6.5. Luồng Admin (Quản trị viên)

```mermaid
flowchart TD
    subgraph ADMIN ["Admin Dashboard"]
        A1[Bảng điều khiển] --> A2{Menu chức năng}

        A2 --> B1[Quản lý Ngân hàng câu hỏi]
        A2 --> C1[Quản lý Phòng thi]
        A2 --> D1[Đăng xuất]
    end

    D1 --> E1[Login Screen]

    subgraph BANK ["Quản lý Ngân hàng"]
        B1 --> B2{Chọn thao tác}
        B2 --> B3[Import từ CSV]
        B2 --> B4[Chỉnh sửa câu hỏi]
        B2 --> B5[Xóa ngân hàng]
        B2 --> B6[Tạo mới]

        B3 --> B7[Parse CSV - JSON]
        B7 --> B8[Gửi IMPORT_QUESTIONS]

        B4 --> B9[Mở Editor Grid]
        B9 --> B10[Chỉnh sửa trong GtkListStore]
        B10 --> B11[Gửi UPDATE_QUESTION_BANK]
    end

    subgraph ROOM_MGMT ["Quản lý Phòng thi"]
        C1 --> C2{Chọn thao tác}
        C2 --> C3[Tạo phòng mới]
        C2 --> C4[Đóng phòng]
        C2 --> C5[Xóa phòng]

        C3 --> C6[Chọn ngân hàng câu hỏi]
        C6 --> C7[Cấu hình thời gian]
        C7 --> C8[Số lượt thi tối đa]
        C8 --> C9[Gửi CREATE_ROOM]
    end

    style ADMIN fill:#F3E5F5,stroke:#7B1FA2
    style BANK fill:#E1F5FE,stroke:#0288D1
    style ROOM_MGMT fill:#FBE9E7,stroke:#E64A19
    style B3 fill:#4CAF50,color:#fff
    style B5 fill:#f44336,color:#fff
    style C3 fill:#4CAF50,color:#fff
    style C4 fill:#FF9800,color:#fff
    style C5 fill:#f44336,color:#fff
    style E1 fill:#607D8B,color:#fff
```

#### 6.6. Luồng Chi tiết - Làm bài thi

```mermaid
sequenceDiagram
    participant U as User
    participant UI as Client UI
    participant NET as Network Layer
    participant S as Server

    Note over U, S: Từ Room Detail Screen

    U->>UI: Click "Bắt đầu"
    UI->>NET: send_request(JOIN_ROOM)
    NET->>S: REQ (JOIN_ROOM, room_id)

    S->>S: Kiểm tra quyền & lượt thi
    S-->>NET: RES (questions, duration)
    NET-->>UI: on_network_event()
    UI->>UI: transition_window(EXAM)

    Note over UI: Exam Screen hiển thị
    UI->>UI: Khởi động Timer (g_timeout_add)

    loop Mỗi câu hỏi
        UI->>U: Hiển thị câu hỏi + 4 đáp án
        U->>UI: Chọn đáp án (Radio Button)
        UI->>NET: send_request(SUBMIT_ANSWER)
        NET->>S: REQ (answer_data)
        S->>S: Lưu vào ExamSession
        S-->>NET: RES (SUCCESS)
        U->>UI: Click "Tiếp theo"
    end

    alt Nộp bài thủ công
        U->>UI: Click "Nộp bài"
    else Hết giờ
        UI->>UI: Timer callback
    end

    UI->>NET: send_request(FINISH_EXAM)
    NET->>S: REQ (FINISH_EXAM)
    S->>S: Tính điểm
    S-->>NET: RES (score, total)
    NET-->>UI: handle_server_message()
    UI->>UI: transition_window(HOME)
    UI->>U: Hiển thị kết quả
```

#### 6.7. Cơ chế Chuyển màn hình (Window Transition)

```mermaid
flowchart LR
    subgraph UI_LAYER ["ui.c - Central Controller"]
        TW[transition_window]
    end

    subgraph SCREENS ["Các màn hình"]
        L[ui_login.c]
        H[ui_home.c]
        A[ui_admin.c]
        R[ui_room_detail.c]
        E[ui_exam.c]
    end

    TW -->|"destroy old"| OLD[gtk_widget_destroy]
    TW -->|"create new"| L
    TW -->|"create new"| H
    TW -->|"create new"| A
    TW -->|"create new"| R
    TW -->|"create new"| E

    NET[on_network_event] -->|"trigger"| TW
    USER[User Action] -->|"trigger"| TW

    style UI_LAYER fill:#FFECB3,stroke:#FFA000
    style SCREENS fill:#E8EAF6,stroke:#3F51B5
    style TW fill:#FF9800,color:#fff
    style NET fill:#2196F3,color:#fff
    style USER fill:#4CAF50,color:#fff
    style OLD fill:#f44336,color:#fff
```

**Đặc điểm kỹ thuật:**
- **GTK Main Loop**: `gtk_main()` xử lý tất cả sự kiện UI và timer
- **Network Integration**: Callback `on_network_event` được gọi khi có message từ server
- **State Management**: Trạng thái UI được quản lý thông qua global variables và GTK widgets
- **Timer**: `g_timeout_add()` được sử dụng cho đồng hồ đếm ngược trong Exam Screen
