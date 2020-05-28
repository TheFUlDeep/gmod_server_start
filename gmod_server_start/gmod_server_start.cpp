#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <strstream>

#include <chrono>
#include <thread>


#define _WIN32_WINNT 0x0A00
#define BOOST_DATE_TIME_NO_LIB
#define BOOST_REGEX_NO_LIB

#include <boost/asio.hpp>


#include <boost/beast.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

using namespace std;
namespace http = boost::beast::http;

typedef stack<string> argsstack;
const auto STRINGNPOS = string::npos;

argsstack FileLinesToStack(const char *file)
{
    argsstack stck;
    ifstream f(file);
    if (f.is_open()) 
    {
        string line;
        while (getline(f, line)) stck.push(line);
        f.close();
    }
    return stck;
}

const char* argsminus[]{ "insecure","authkey" };
const char* argsplus[]{ "sv_setsteamaccount","host_workshop_collection","maxplayers","map" };

const string GetStartArgs()
{
    string str = "";

    auto linesstack = FileLinesToStack("server\\startargs.txt");

    while (!linesstack.empty())
    {
        const string line = linesstack.top();
        linesstack.pop();

        //пропускаю закомментированные строки или неправильные строки (в которых нет знака =) или FALSE
        if (line[0] == '#' || line.find("=FALSE") != STRINGNPOS || line.find(' ') != STRINGNPOS || line.find('+') != STRINGNPOS || line.find('-') != STRINGNPOS) continue;
        const auto equalpos = line.find('=');
        if (equalpos == STRINGNPOS) continue;

        //если в правой части есть + или -, то игнорить эту строку (для безопасности, чтобы нельзя было дописать свои аргументы)
        //возможно лучше делать проверку на + и - с пробелом, но если ни в одной команде нет ни + не -, то проще не менять
        //if (eqright.find('+') != STRINGNPOS || eqright.find('-') != STRINGNPOS /*|| eqright.find('=') != STRINGNPOS*/) continue; //закомментил проверку на второй знак =. Хз, надо его или нет

        const string eqleft = line.substr(0, equalpos);

        for (string word : argsminus)
        {
            if (eqleft != word) continue;
            str += ('-' + eqleft + ' ');
            if (line.find("=TRUE") == STRINGNPOS) str += (line.substr(equalpos + 1) + ' ');
        }

        for (string word : argsplus)
        {
            if (eqleft != word) continue;
            str += ('+' + eqleft + ' ');
            if (line.find("=TRUE") == STRINGNPOS) str += (line.substr(equalpos + 1) + ' ');
        }

    }

    return str;
}

string port = "";
string windowtitle1 = "";
void StartServer(char** argv)
{
    string name = argv[0];
    auto backslash = name.rfind("\\");
    if (backslash != STRINGNPOS) name = name.substr(backslash + 1);

    setlocale(LC_ALL, "english");

    auto adminargsstack = FileLinesToStack("adminargs.txt");
    const string affinity = adminargsstack.top(); adminargsstack.pop();
    port = adminargsstack.top(); adminargsstack.pop();

    windowtitle1 = name + " server watchdog";
    const string start = ("@echo off\ncls\necho Protecting srcds from crashes...\ntitle " + windowtitle1 + "\n:srcds\necho (%date% %time%) srcds started.\n");
    const string prestartargs = (start + "cd ./server\nstart /wait /high /min /affinity " + affinity + " srcds.exe -nocrashdialog -console -tickrate 20 -autoupdate -game garrysmod -port " + port + " ");
    const string end = "\necho (%date% %time%) WARNING: srcds closed or crashed, restarting.\ngoto srcds";

    const string startargs = GetStartArgs();

    const string batcommand = (prestartargs + startargs + end);


    ofstream f("start.bat");
    if (f.is_open())
    {
        f << batcommand;
        f.close();
        //cout << "added start" << endl;
    }

    //system("pause");
    system("start start.bat");
    system(string("echo (%date% %time%) " + windowtitle1 + " started").c_str());
    //-condebug
}

const string GetMyIP()
{
    ostringstream os;
    boost::asio::ip::tcp::iostream stream("httpbin.org", "http");
    stream << "GET /ip HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    os << stream.rdbuf();
    string str = os.str();

    auto originpos = str.find("origin");
    if (originpos != STRINGNPOS)
    {
        str = str.substr(originpos + 10);
        auto secondpos = str.find('"');
        str = str.substr(0, secondpos);
        return str;
    }
    return "";
}

char bad_responses = 0;

void CheckServerInOnline()
{
    this_thread::sleep_for(std::chrono::seconds(60));

    /*
    boost::asio::ip::tcp::iostream stream("api.steampowered.com", "http");
    stream << "GET /ip HTTP/1.1\r\nHost: api.steampowered.com\r\nConnection: close\r\n\r\n";
    cout << stream.rdbuf();
    */

    auto myip = GetMyIP();
    if (myip == "") return;
    string fulladdr = myip + ":" + port;

    const std::string host = "api.steampowered.com";
    const std::string target = "/ISteamApps/GetServersAtAddress/v1/?addr=" + fulladdr;

    // I/O контекст, необходимый для всех I/O операций
    boost::asio::io_context ioc;

    // Resolver для определения endpoint'ов
    boost::asio::ip::tcp::resolver resolver(ioc);
    // Tcp сокет, использующейся для соединения
    boost::asio::ip::tcp::socket socket(ioc);

    // Резолвим адрес и устанавливаем соединение
    boost::asio::connect(socket, resolver.resolve(host, "80"));

    // Дальше необходимо создать HTTP GET реквест с указанием таргета
    http::request<http::string_body> req(http::verb::get, target, 11);
    // Задаём поля HTTP заголовка
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Отправляем реквест через приконекченный сокет
    http::write(socket, req);

    // Часть, отвечающая за чтение респонса
    {
        boost::beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(socket, buffer, res);

        ostringstream os;
        os << res;
        string str = os.str();

        if (str.find(fulladdr) == STRINGNPOS)
        {
            bad_responses++;
            if (bad_responses == 5)
            {
                string command = string("taskkill /F /FI ") + '"' + "WINDOWTITLE eq " + windowtitle1 + '"' + " /T";
                system("echo (%date% %time%) server didnt rersponse for 5 times. Restarting...");
                system(command.c_str());
                system("start start.bat");
                bad_responses = 0;
            }
        }
        else bad_responses = 0;
    }
    // Закрываем соединение
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);

}

int main(int argc, char** argv)
{
    bool notprogramend = true;
    StartServer(argv);

    thread t([&notprogramend]() 
    {
            while (notprogramend)
            {
                char s;
                cin >> s;
                if (s == 's') notprogramend = false;
            }
    });

    while (notprogramend) CheckServerInOnline();

    t.join();

    return 0;
}