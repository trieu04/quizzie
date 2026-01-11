# Participant Room Spec

## 1. Requirements

### 3.1.3. Tham gia thi (Dành cho Participant)
- **REQ-QUIZ-01 (Xem danh sách phòng)**: Người dùng xem được danh sách các phòng thi đang mở, trạng thái phòng (Waiting, Open, Closed).
  - Open Time/Close Time: Khoảng thời gian cho phép người dùng **vào phòng và bắt đầu thi**.
  - Duration: Thời gian làm bài thi (ví dụ: 30 phút).
  - Nếu phòng trong trạng thái Waiting -> Người dùng không thể tham gia vào phòng (chưa mở).
  - Nếu phòng trong trạng thái Closed -> Người dùng không thể tham gia (đã kết thúc).
- **REQ-QUIZ-03 (Làm bài thi)**:
  - Hiển thị câu hỏi và 4 đáp án lựa chọn.
  - Đồng hồ đếm ngược thời gian làm bài (dựa trên Duration).
  - Gửi đáp án đã chọn lên Server ngay lập tức để lưu trạng thái.
- **REQ-QUIZ-04 (Nộp bài)**: Người dùng có thể nộp bài trước khi hết giờ. Hệ thống tự động thu bài khi hết giờ.

## 2. UI Design (Concept)

### 2.1. Room List Screen
- **Components**:
    - Table/List hiển thị các phòng active.
    - Cột: Tên phòng, ID, Trạng thái.
    - Button "Join" để tham gia.

### 2.2. Exam Screen
- **Components**:
    - **Top Bar**: Timer đếm ngược, số câu đã làm, số câu còn lại.
    - **Main Content**: 
        - Nội dung câu hỏi (Text/Image).
        - 4 Button đáp án (A, B, C, D).
    - **Control**: Button "Submit".

## 3. Logic & Flow

1. **List Rooms**:
    - Client gửi Request `LIST_ROOMS`.
    - Server trả về danh sách.
    - User chọn phòng -> Gửi `JOIN_ROOM`.

2. **Starting Exam**:
    - Khi User join thành công vào phòng Open -> Server trả về danh sách câu hỏi (randomized) kèm thời gian làm bài.
    - Server lưu danh sách câu hỏi đề thi của user này vào Database/File để chấm điểm sau này.
    - Client nhận danh sách -> Chuyển màn hình Exam -> Start timer.

3. **Taking Exam**:
    - User chọn đáp án -> Client gửi `SUBMIT_ANSWER`.
    - Server lưu đáp án.
    - Client disable các lựa chọn đã chọn hoặc cho phép đổi (tùy rule).
    - Khi hết giờ (Client timer = 0 hoặc Server push `TIMEOUT`) -> Disable nhập liệu.

4. **Finishing**:
    - Hết câu hỏi hoặc hết giờ làm bài -> Server tính điểm -> Gửi `EXAM_RESULT`.


## Note:
- danh sách câu hỏi và câu trả lời nối tiếp nhau dùng radio button
- Cần có nút rời phòng thi, và có thể tiếp tục tham gia phòng thi
- Cần thêm 1 màn hình: Room detail, trong đó có
    - Tất cả các thông tin thuộc tính của room
    - Lịch sử và kết quả làm bài
    - Các nút hành động: "Start Quiz" (cần confirm thời gian),...
