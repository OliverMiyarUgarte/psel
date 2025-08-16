#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <string>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

using namespace std;

const string RED = "\033[31m";
const string GREEN = "\033[32m";
const string YELLOW = "\033[33m";
const string BLUE = "\033[34m";
const string MAGENTA = "\033[35m";
const string CYAN = "\033[36m";
const string RESET = "\033[0m";

void print_banner() {
    cout << CYAN <<
        "\n  /\\_/\\  \n"
        " ( " << MAGENTA << "◕‿◕" << CYAN << " )  " << YELLOW << "✧ Servidor de Arquivos ✧" << RESET << "\n"
        "  /   \\ \n"
        " /     \\ \n\n" << RESET;
}

string get_content_type(const string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css")) return "text/css";
    if (path.ends_with(".js")) return "application/javascript";
    if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
    if (path.ends_with(".png")) return "image/png";
    if (path.ends_with(".gif")) return "image/gif";
    return "text/plain";
}

bool handle_file_upload(const string& body, const string& boundary, const string& web_root) {
    string boundary_marker = "--" + boundary;
    string end_boundary_marker = boundary_marker + "--";

    size_t part_start = body.find(boundary_marker);
    if (part_start == string::npos) return false;
    part_start += boundary_marker.size();

    size_t header_end = body.find("\r\n\r\n", part_start);
    if (header_end == string::npos) return false;

    string part_header = body.substr(part_start, header_end - part_start);
    string filename = "arquivoupload.html";
    size_t fn_pos = part_header.find("filename=\"");
    if (fn_pos != string::npos) {
        fn_pos += 10;
        size_t fn_end = part_header.find("\"", fn_pos);
        if (fn_end != string::npos) {
            filename = part_header.substr(fn_pos, fn_end - fn_pos);
            size_t slash = filename.find_last_of("/\\");
            if (slash != string::npos)
                filename = filename.substr(slash + 1);
        }
    }

    size_t data_start = header_end + 4;
    size_t data_end = body.find(boundary_marker, data_start);
    if (data_end == string::npos) {
        data_end = body.find(end_boundary_marker, data_start);
        if (data_end == string::npos) return false;
    }
    if (data_end >= 2 && body.substr(data_end - 2, 2) == "\r\n") {
        data_end -= 2;
    }

    string full_path = web_root + filename;
    ofstream output_file(full_path, ios::binary);
    if (!output_file) {
        cerr << RED << "Nao deu para criar: " << filename << RESET << endl;
        return false;
    }
    output_file.write(body.data() + data_start, data_end - data_start);
    output_file.close();
    cout << GREEN << "Salvou: " << filename << RESET << endl;
    return true;
}

void handle_request(int client_sock, const string& web_root) {
    char buffer[4096] = {0};
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        cerr << RED << "✧ Erro no pedido! (´；ω；｀) ✧\n" << RESET;
        return;
    }

    string request(buffer, bytes_received);
    istringstream iss(request);
    string method, path, version;
    iss >> method >> path >> version;

    if (method == "POST" && path == "/upload") {
        cout << GREEN << "✧ Iniciando o upload de arquivo ✧\n" << RESET;

        size_t content_length = 0;
        size_t cl_pos = request.find("Content-Length:");
        if (cl_pos != string::npos) {
            cl_pos += 15;
            while (cl_pos < request.size() && isspace(request[cl_pos])) ++cl_pos;
            size_t cl_end = request.find("\r\n", cl_pos);
            string cl_val = request.substr(cl_pos, cl_end - cl_pos);
            content_length = stoi(cl_val);
        }

        string boundary;
        size_t boundary_pos = request.find("boundary=");
        if (boundary_pos != string::npos) {
            size_t b_start = boundary_pos + 9;
            size_t b_end = request.find("\r\n", b_start);
            if (b_end == string::npos) b_end = request.find(';', b_start);
            boundary = request.substr(b_start, b_end - b_start);
        } else {
            cerr << RED << "✧ Sem boundary! ✧\n" << RESET;
            string response = "HTTP/1.1 400 \r\nContent-Type: text/html\r\n\r\n"
                              "<html><body><h1>400 </h1><p>Boundary nao especifica.</p></body></html>";
            send(client_sock, response.c_str(), response.size(), 0);
            return;
        }

        size_t header_end = request.find("\r\n\r\n");
        string body;
        if (header_end != string::npos) {
            body = request.substr(header_end + 4);
        } else {
            cerr << RED << "✧ Malformataado! ✧\n" << RESET;
            return;
        }

        while (body.size() < content_length) {
            char temp_buf[4096];
            int n = recv(client_sock, temp_buf, min(sizeof(temp_buf), content_length - body.size()), 0);
            if (n <= 0) break;
            body.append(temp_buf, n);
        }

        if (!handle_file_upload(body, boundary, web_root)) {
            cerr << RED << "✧ Falha no upload! (╥﹏╥) ✧\n" << RESET;
            string response = "HTTP/1.1 500 Error\r\nContent-Type: text/html\r\n\r\n"
                              "<html><body><h1>Erro no upload!</h1></body></html>";
            send(client_sock, response.c_str(), response.size(), 0);
            return;
        }

        string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                          "<html><body><h1>Arquivo enviado com sucesso!</h1></body></html>";
        send(client_sock, response.c_str(), response.size(), 0);
        return;
    }

    if (path == "/") path = "/index.html";

    cout << MAGENTA << "✧ Pedido por: " << YELLOW << path << MAGENTA << " ✧\n" << RESET;

    string full_path = web_root + path;

    if (fs::is_directory(full_path)) {
        full_path += "/index.html";
    }

    ifstream file(full_path, ios::binary);
    if (!file) {
        string response = "HTTP/1.1 404 Nao achado\r\nContent-Type: text/html\r\n\r\n"
                          "<html><body><h1>404 Nao achado</h1><p>Arquivo nao foi encontrado.</p></body></html>";
        send(client_sock, response.c_str(), response.size(), 0);
        return;
    }

    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    string content_type = get_content_type(path);

    string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + content_type + "\r\n";
    response += "Content-Length: " + to_string(content.size()) + "\r\n";
    response += "Server: Servidor de Arquivos\r\n\r\n";
    response += content;

    cout << GREEN << "✧ Servindo Arquivo: " << YELLOW << path << GREEN << " ✧\n" << RESET;
    send(client_sock, response.c_str(), response.size(), 0);
}

int main() {
    print_banner();

    string web_root;
    cout << BLUE << "Oi! " << MAGENTA << "✧ Qual diretorio deveria usar? " << YELLOW << "(e.g., ./public): " << RESET;
    getline(cin, web_root);
    if (!web_root.empty() && web_root.back() != '/') {
        web_root += '/';
    }

    cout << GREEN << "\n✧ Servindo arquivos de: " << YELLOW << web_root << GREEN << " ✧\n\n" << RESET;

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        cerr << RED << "✧ Erro criando o socket! (╥﹏╥) ✧\n" << RESET;
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << RED << "✧ Erro no bind! (´；ω；｀) ✧\n" << RESET;
        close(server_sock);
        return 1;
    }

    listen(server_sock, 5);
    cout << CYAN << "✧ Escutando no " << YELLOW << "http://localhost:8080" << CYAN << " ✧\n" << RESET;

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            cerr << RED << "✧ Conexao falhou! (｡•́︿•̀｡) ✧\n" << RESET;
            continue;
        }

        cout << GREEN << "✧ Nova conexao! (≧◡≦) ✧\n" << RESET;
        handle_request(client_sock, web_root);
        close(client_sock);
        cout << MAGENTA << "✧ Concluido! (ᵔ◡ᵔ) ✧\n\n" << RESET;
    }

    close(server_sock);
    return 0;
}
