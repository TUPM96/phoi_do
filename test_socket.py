import socket

HOST = '192.168.31.10'  # Đổi thành IP của server bạn (hoặc localhost nếu chạy cùng máy)
PORT = 3000             # Port theo đúng server

def main():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, PORT))
            print(f'Đã kết nối tới {HOST}:{PORT}')
            while True:
                cmd = input('Nhập lệnh gửi (MOTOR ... hoặc GOTO_KHO ... hoặc STOP, q để thoát): ')
                if cmd.lower() == 'q':
                    print('Thoát.')
                    break
                s.sendall(cmd.encode('utf-8'))
                data = s.recv(1024)
                print('Phản hồi từ server:', data.decode('utf-8'))
    except Exception as e:
        print('Lỗi:', e)

if __name__ == "__main__":
    main()