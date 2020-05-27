#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <strstream>

#include <chrono>
#include <thread>

#include "HTTPRequest.hpp"


using namespace std;

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

void StartServer(char** argv)
{
    string name = argv[0];
    auto backslash = name.rfind("\\");
    if (backslash != string::npos) name = name.substr(backslash + 1);

    setlocale(LC_ALL, "english");

    auto adminargsstack = FileLinesToStack("adminargs.txt");
    const string affinity = adminargsstack.top(); adminargsstack.pop();
    port = adminargsstack.top(); adminargsstack.pop();

    string windowtitle1 = name + " server watchdog";
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

void CheckServerInOnline()
{
    this_thread::sleep_for(std::chrono::seconds(10));
    const string addr = "https://api.steampowered.com/";
    //auto response = cpr::Get(cpr::Url{ "https://api.steampowered.com/ISteamApps/GetServersAtAddress/v1/?addr=77.37.178.206:27015" });

    try
    {
        // you can pass http::InternetProtocol::V6 to Request to make an IPv6 request
        http::Request request(addr);

        // send a get request
        const http::Response response = request.send("GET");
        std::cout << std::string(response.body.begin(), response.body.end()) << '\n'; // print the result
    }
    catch (const std::exception& e)
    {
        std::cerr << "Request failed, error: " << e.what() << '\n';
    }

}

int main(int argc, char** argv)
{
    //StartServer(argv);
    while (true) CheckServerInOnline();
    return 0;
}