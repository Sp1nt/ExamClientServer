#include <winsock2.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

#define MAX_CLIENTS 10
#define DEFAULT_BUFLEN 4096

#pragma comment(lib, "ws2_32.lib") // Winsock library
#pragma warning(disable:4996) // отключаем предупреждение _WINSOCK_DEPRECATED_NO_WARNINGS

SOCKET server_socket;

vector<string> history;

struct MainMenu {
    string name, category;
    float price;
};

MainMenu* menu;
int order = 0;
int wait = 0;
float total_bill = 0.0;


void readMenuFromFile(const string& filename) {
    ifstream file(filename);
    if (file.is_open()) {
        stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        string jsonString = buffer.str();

        size_t pos = 0;
        int products = 0;
        while ((pos = jsonString.find("name", pos)) != string::npos) {
            products++;
            pos += 4;
        }

        menu = new MainMenu[products];
        pos = 0;
        int i = 0;

        while ((pos = jsonString.find("name", pos)) != string::npos) {
            size_t nameEnd = jsonString.find(",", pos + 4);
            menu[i].name = jsonString.substr(pos + 8, nameEnd - pos - 9);

            size_t categoryStart = jsonString.find("category", nameEnd);
            size_t categoryEnd = jsonString.find(",", categoryStart + 12);
            menu[i].category = jsonString.substr(categoryStart + 12, categoryEnd - categoryStart - 13);

            size_t priceStart = jsonString.find("price", categoryEnd);
            size_t priceEnd = jsonString.find(",", priceStart + 8);
            menu[i++].price = stof(jsonString.substr(priceStart + 8, priceEnd - priceStart - 8));
            pos = priceEnd;
        }
        order = products;
    }
    else {
        cout << "Unable to open file " << filename << endl;
    }
}

string TotalTimeAndSum(string request) {
    for (int i = 0; i < order; i++) {
        if (request.find(menu[i].name) != string::npos) {
            if (menu[i].category == "Burgers")
                wait += 5;
            else if (menu[i].category == "Sides")
                wait += 3;
            else if (menu[i].category == "Drinks")
                wait += 1;
        }
    }
    if (wait != 0) {
        total_bill = 0.0;
        for (int i = 0; i < order; i++) {
            if (request.find(menu[i].name) != string::npos) {
                total_bill += menu[i].price;
            }
        }
        return "Please wait for " + to_string(wait) + " seconds";
    }
    else {
        return "Please choose something from the menu";
    }
}

int main() {
    system("title Server");

    puts("Start server... DONE.");
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code: %d", WSAGetLastError());
        return 1;
    }

    // Читаем меню из файла
    readMenuFromFile("menu.txt");

    // Создаем сокет
    SOCKET server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket: %d", WSAGetLastError());
        return 2;
    }

    // Подготавливаем структуру sockaddr_in
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);

    // Привязываем сокет
    if (bind(server_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed with error code: %d", WSAGetLastError());
        return 3;
    }

    // Слушаем входящие соединения
    listen(server_socket, MAX_CLIENTS);

    // Принимаем входящее соединение
    puts("Server is waiting for incoming connections...\nPlease, start one or more client-side app.");

    // Набор дескрипторов сокетов
    fd_set readfds;
    SOCKET client_socket[MAX_CLIENTS] = {};

    while (true) {
        // Очищаем набор дескрипторов
        FD_ZERO(&readfds);

        // Добавляем главный сокет в набор
        FD_SET(server_socket, &readfds);

        // Добавляем дочерние сокеты в набор
        for (int i = 0; i < MAX_CLIENTS; i++) {
            SOCKET s = client_socket[i];
            if (s > 0) {
                FD_SET(s, &readfds);
            }
        }

        // Ожидаем активности на любом из сокетов
        if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) {
            printf("select function call failed with error code : %d", WSAGetLastError());
            return 4;
        }

        // Если что-то произошло на главном сокете, это входящее соединение
        SOCKET new_socket;
        sockaddr_in address;
        int addrlen = sizeof(sockaddr_in);
        if (FD_ISSET(server_socket, &readfds)) {
            if ((new_socket = accept(server_socket, (sockaddr*)&address, &addrlen)) < 0) {
                perror("accept function error");
                return 5;
            }


            // Информируем о подключении нового клиента
            printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Добавляем новый сокет в массив сокетов
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets at index %d\n", i);
                    break;
                }
            }
        }

        // Если какой-то из клиентских сокетов отправляет что-то
        for (int i = 0; i < MAX_CLIENTS; i++) {
            SOCKET s = client_socket[i];
            // Если клиент присутствует в наборе для чтения
            if (FD_ISSET(s, &readfds)) {
                // Получаем реквизиты клиента
                getpeername(s, (sockaddr*)&address, (int*)&addrlen);

                // Проверяем, было ли отправлено сообщение "off", что указывает на отключение клиента
                char client_message[DEFAULT_BUFLEN];
                int client_message_length = recv(s, client_message, DEFAULT_BUFLEN, 0);
                client_message[client_message_length] = '\0';
                string check_exit = client_message;
                if (check_exit == "off") {
                    cout << "Client #" << i << " is off\n";
                    client_socket[i] = 0;
                }
                string client_request = client_message;
                // Генерируем ответное сообщение на основе заказа клиента
                string response = TotalTimeAndSum(client_message);
                send(s, response.c_str(), response.size(), 0);

                Sleep(wait * 1000);

                if (wait != 0) {
                    total_bill = 0.0;
                    for (int i = 0; i < order; i++) {
                        if (client_request.find(menu[i].name) != string::npos) {
                            total_bill += menu[i].price;
                        }
                    }
                    string bill_message = "Your total bill is $" + to_string(total_bill) + ". Good Luck!";
                    send(s, bill_message.c_str(), bill_message.size(), 0);
                    wait = 0;
                }
                

                // Записываем историю сообщений
                string temp = client_message;
                history.push_back(temp);
            }
        }
    }

    WSACleanup();
    return 0;
}
