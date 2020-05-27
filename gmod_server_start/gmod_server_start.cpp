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

argsstack FileLinesToStack(const char *file)
{
    argsstack stck;
    std::ifstream f(file);
    if (f.is_open()) 
    {
        std::string line;
        while (std::getline(f, line)) stck.push(line);
        f.close();
    }
    return stck;
}

const char* argsminus[]{ "insecure","authkey" };
const char* argsplus[]{ "sv_setsteamaccount","host_workshop_collection","maxplayers","map","gamemode" };

const string GetStartArgs()
{
    string str = "";

    auto linesstack = FileLinesToStack("server\\startargs.txt");

    while (!linesstack.empty())
    {
        string line = linesstack.top();
        linesstack.pop();

        //пропускаю закомментированные строки или неправильные строки (в которых нет знака =) или FALSE
        if (line[0] == '#' || line.find("=FALSE") != string::npos) continue;
        size_t equalpos = line.find('=');
        if (equalpos == string::npos) continue;

        for (string word : argsminus)
        {
            if (line.find(word) == string::npos) continue;
            string leftpart = '-' + word;
            string rightpart;
            if (line.find("=TRUE") != string::npos) rightpart = "";
            else rightpart = ' ' + line.substr(equalpos + 1);
            str += (leftpart + rightpart + ' ');
        }

        for (string word : argsplus)
        {
            if (line.find(word) == string::npos) continue;
            string leftpart = '+' + word;
            string rightpart;
            if (line.find("=TRUE") != string::npos) rightpart = "";
            else rightpart = ' ' + line.substr(equalpos + 1);
            str += (leftpart + rightpart + ' ');
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
    if (backslash != string::npos) name = name.substr(backslash + 1);

    setlocale(LC_ALL, "english");

    auto adminargsstack = FileLinesToStack("adminargs.txt");
    const string affinity = adminargsstack.top(); adminargsstack.pop();
    port = adminargsstack.top(); adminargsstack.pop();

    windowtitle1 = name + " server watchdog";
    const string start = ("@echo off\ncls\necho Protecting srcds from crashes...\ntitle " + windowtitle1 + "\n:srcds\necho (%date%) (%time%) srcds started.\n");
    const string prestartargs = (start + "cd ./server\nstart /wait /high /min /affinity " + affinity + " srcds.exe -nocrashdialog -console -tickrate 20 -autoupdate -game garrysmod -port " + port + " ");
    const string end = "\necho (%date%) (%time%) WARNING: srcds closed or crashed, restarting.\ngoto srcds";

    const string startargs = GetStartArgs();

    const string batcommand = (prestartargs + startargs + end);


    ofstream f("start.bat");
    if (f.is_open())
    {
        f << batcommand;
        f.close();
        //cout << "added start" << endl;
    }

    string windowtitle2 = name + " start_of_start";
    f = ofstream("start_of_start.bat");
    if (f.is_open())
    {
        f << string("@echo off\ncls\necho Protecting " + windowtitle1 + " from crashes...\ntitle " + windowtitle2 + "\n:srcds\necho (%date%) (%time%) " + windowtitle1 + " started.\nstart /wait start.bat\necho (%date%) (%time%) WARNING : " + windowtitle1 + " closed or crashed, restarting.\ngoto srcds");
        f.close();
        //cout << "added start_of_start" << endl;
    }

    //system("pause");
    system("start start_of_start.bat");
    //-condebug
}

const string GetMyIP()
{
    std::ostringstream os;
    boost::asio::ip::tcp::iostream stream("httpbin.org", "http");
    stream << "GET /ip HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    os << stream.rdbuf();
    string str = os.str();

    auto originpos = str.find("origin");
    if (originpos != string::npos)
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

        std::ostringstream os;
        os << res;
        string str = os.str();

        if (str.find(fulladdr) == string::npos)
        {
            bad_responses++;
            if (bad_responses == 5)
            {
                string command = string("taskkill /F /FI ") + '"' + "WINDOWTITLE eq " + windowtitle1 + '"' + " /T";
                system(command.c_str());
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