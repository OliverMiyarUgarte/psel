#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <string>
#include <netdb.h>
#include <map>

using namespace std;

// Cores ANSI
const string RED = "\033[31m";
const string GREEN = "\033[32m";
const string YELLOW = "\033[33m";
const string BLUE = "\033[34m";
const string MAGENTA = "\033[35m";
const string CYAN = "\033[36m";
const string RESET = "\033[0m";

// Pedido Http
struct HttpPed {
    string method;
    string path;
    string version;
    map<string, string> headers;
};

HttpPed PedParse(const string& PedPuro) {
    HttpPed ped;
    istringstream iss(PedPuro);
    
    // Parse do http
    getline(iss, ped.method, ' ');
    getline(iss, ped.path, ' ');
    getline(iss, ped.version, '\r');
    
    // Parse headers
    string line;
    while (getline(iss, line) && line != "\r") {
        size_t colon = line.find(':');
        if (colon != string::npos) {
            string key = line.substr(0, colon);
            string value = line.substr(colon + 2);
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            ped.headers[key] = value;
        }
    }
    return ped;
}

void banner() {
    cout << CYAN << 
        "\n  /\\_/\\  \n"
        " ( " << MAGENTA << "o.o" << CYAN << " )  " << YELLOW << "Reverse Proxy Trevoso" << RESET << "\n"
        "  > " << RED << "^" << CYAN << " <   " << RESET << "\n"
        "  /   \\ \n"
        " /     \\ \n\n" << RESET;
}

int main() {
    banner();

    // Socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << RED << "Falhou :3\n" << RESET;
        return 1;
    }

    // Bind e config
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3000);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << RED << "Bind falhou! :3\n" << RESET;
        close(sock);
        return 1;
    }

    listen(sock, 5);
    cout << CYAN << "Escutando no " << YELLOW << "http://localhost:3000" << "\n" << RESET;
    cout << RED << "Ctrl+C para me matar (｡•́︿•̀｡)\n\n" << RESET;

    while (true) {
        // Conexao
        sockaddr_in AddrCliente;
        socklen_t LenCliente = sizeof(AddrCliente);
        int SockCliente = accept(sock, (struct sockaddr*)&AddrCliente, &LenCliente);
        if (SockCliente < 0) {
            cerr << RED << "Falhou ;3\n" << RESET;
            continue;
        }

        // Request
        char buffer[4096] = {0};
        int Recebido = recv(SockCliente, buffer, sizeof(buffer) - 1, 0);
        if (Recebido < 0) {
            cerr << RED << "Erro ;3\n" << RESET;
            close(SockCliente);
            continue;
        }

        // Parsing
        string PedPuro(buffer, Recebido);
        HttpPed pedido = PedParse(PedPuro);

        cout << BLUE << "\nRecebido " << YELLOW << pedido.method << BLUE << " pedido para "  << MAGENTA << pedido.path << BLUE << "\n" << RESET;

        // Conexao Back
        int SockBack = socket(AF_INET, SOCK_STREAM, 0);
        if (SockBack < 0) {
            cerr << RED << "Falhou ;3\n" << RESET;
            close(SockCliente);
            continue;
        }

        sockaddr_in BackAddr;
        BackAddr.sin_family = AF_INET;
        BackAddr.sin_port = htons(8080);
        BackAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(SockBack, (sockaddr*)&BackAddr, sizeof(BackAddr)) < 0) {
            cerr << RED << "Falhou ;3\n" << RESET;
            close(SockCliente);
            close(SockBack);
            continue;
        }

        // Headar
        size_t HostPos = PedPuro.find("Host: ");
        if (HostPos != string::npos) {
            size_t HostEnd = PedPuro.find("\r\n", HostPos);
            PedPuro.replace(HostPos, HostEnd - HostPos, "Host: localhost");
        }

        cout << GREEN << "Proxy para " << YELLOW << "Local Host" << GREEN << "!\n" << RESET;
        send(SockBack, PedPuro.c_str(), PedPuro.size(), 0);

        // Devolver Resposta
        char BackBuff[4096];
        int BackResp;
        while ((BackResp = recv(SockBack, BackBuff, sizeof(BackBuff), 0)) > 0) {
            send(SockCliente, BackBuff, BackResp, 0);
        }

        cout << MAGENTA << "pedido completo!\n" << RESET;
        
        close(SockBack);
        close(SockCliente);
    }

    close(sock);
    return 0;
}
