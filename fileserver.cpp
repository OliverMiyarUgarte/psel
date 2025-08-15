#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <string>
#include <fstream>
#include <filesystem>

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

void handle_request(int client_sock, const string& web_root) {
    char buffer[4096] = {0};
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        cerr << RED << "✧ Erro no pedido! (´；ω；｀) ✧\n" << RESET;
        return;
    }

    string request(buffer);
    istringstream iss(request);
    string method, path, version;
    iss >> method >> path >> version;

    if (path == "/") path = "/index.html";

    cout << MAGENTA << "✧ Pedido por: " << YELLOW << path << MAGENTA << " ✧\n" << RESET;

    string full_path = web_root + path;

    if (fs::is_directory(full_path)) {
        full_path += "/index.html";
    }

    ifstream file(full_path, ios::binary);
    if (!file) {
        string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
                          "<html><body><h1>404 Not Found</h1><p>The requested file could not be found.</p></body></html>";
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
