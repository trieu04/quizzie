# Admin Room Spec

#### 3.1.2. Quản lý Phòng thi (Dành cho Admin)
- **REQ-ROOM-01 (Tạo phòng)**: Admin có thể tạo phòng thi được đặt tên, thiết lập tên phòng, question bank, thời gian bắt đầu, thời gian kết thúc, số lượng câu hỏi, số lần thi cho phép.
- **REQ-ROOM-02 (Cấu hình đề thi)**: Admin chọn bộ câu hỏi từ ngân hàng câu hỏi.
- **REQ-ROOM-03 (Quản lý phòng)**: Admin có thể xem danh sách phòng, xem chi tiết (stats, kết quả thi), xóa phòng đã tạo.
- **REQ-ROOM-04 (Quản lý câu hỏi)**: Cho phép Import(CSV)/List/Edit/Delete ngân hàng câu hỏi.
- **REQ-ROOM-05 (Xem chi tiết phòng)**: Xem thông tin chi tiết cấu hình phòng, trạng thái, thống kê số lượt thi và bảng kết quả của người tham gia.

## UI:

### 1. Màn hình Dashboard (Admin)
- **Danh sách phòng thi**: Hiển thị bảng danh sách các phòng thi hiện có.
    - Cột: ID, Tên phòng, Trạng thái (Open/Closed), Thời gian bắt đầu, Thời gian kết thúc.
    - Cột: ID, Tên phòng, Trạng thái (Open/Closed), Thời gian bắt đầu, thời gian kết thúc, Số câu hỏi, Số lượt thi cho phép.
- **Nút "Tạo phòng mới"**: Mở Dialog tạo phòng (Inputs: Name, Question Bank, Start Time, End Time, Num Questions, Allowed Attempts).
- **Nút "Room Details"**: Xem chi tiết phòng, status, thống kê (Total attempts, avg score) và danh sách kết quả người thi.
- **Nút "Manage Question Banks"**: Mở Dialog quản lý ngân hàng câu hỏi.

### 2. Màn hình Quản lý Câu hỏi (Dialog)
- **Import Section**:
    - Chọn file CSV, nhập tên Bank -> Upload (Parse & Send JSON).
- **List Section**:
    - Danh sách các Question Bank hiện có.
    - Actions: Edit Selected (Mở Editor Dialog), Delete Selected.
### 3. Editor Dialog (Edit Question Bank)
- Bảng hiển thị danh sách câu hỏi (Editable).
- Thêm dòng, Xóa dòng.
- Save Changes: Gửi update lên Server.

## Logic:

### 1. Protocol Messages
Sử dụng format JSON tiêu chuẩn của hệ thống (HEADER + PAYLOAD).

#### Tạo phòng (Create Room)
- **Request (Client -> Server)**:
    - `MSG_TYPE`: `REQ`
    - `DATA`:
        ```json
        {
            "action": "CREATE_ROOM",
            "data": {
                "room_name": "Final Exam 2024",
                "question_bank_id": "qb_123",
                "start_time": 1704819600,
                "end_time": 1704823200,
                "num_questions": 10,
                "allowed_attempts": 1
            }
        }
        ```
- **Response**: `RES` (Success) hoặc `ERR` (Fail).


#### Quản lý Câu hỏi (Manage Questions)
Các actions: `IMPORT_QUESTIONS`, `LIST_QUESTION_BANKS`, `GET_QUESTION_BANK`, `UPDATE_QUESTION_BANK`, `DELETE_QUESTION_BANK`.

#### Import Questions
Thay vì gửi file binary chunk, Client parse CSV thành các object JSON và gửi lên server.

- **Request Import Questions**:
    - `MSG_TYPE`: `REQ`
    - `DATA`:
        ```json
        {
            "action": "IMPORT_QUESTIONS",
            "data": {
                "bank_name": "Physic_Chapter1",
                "questions": [
                    {
                        "question": "What is gravity?",
                        "options": ["Force", "Candy", "Dream", "None"],
                        "correct_index": 0
                    },
                    {
                         // ... next question
                    }
                ]
            }
        }
        ```
    - *Lưu ý*: Nếu file quá lớn, Client có thể chia thành nhiều request `IMPORT_QUESTIONS` (batching) nhưng vẫn giữ format JSON object này.
- **Response**:
    - `RES`: Trả về `bank_id` hoặc status thành công .

#### Lấy danh sách phòng (List Rooms)
- **Request**:
    - `MSG_TYPE`: `REQ`
    - `DATA`: `{ "action": "LIST_ROOMS" }`
- **Response**:
    - `DATA`: Danh sách mảng các phòng.

### 2. Server Processing
- **Storage**: Lưu thông tin phòng và question bank vào database/file.
- **Logic**:
    - Khi nhận `IMPORT_QUESTIONS`, Server loop qua mảng `questions`, validate từng câu và lưu vào storage.
    - Khi nhận `CREATE_ROOM`, Server tạo room ID mới và lưu thông tin config.
    - Khi nhận `GET_ROOM_STATS`, Server tính toán số lượt thi và điểm trung bình từ kết quả đã lưu.
