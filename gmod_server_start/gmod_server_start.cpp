#include <iostream>
#include <fstream>
#include <string>
#include <stack>
//#include <strstream>

#include <chrono>
#include <thread>


//#define _WIN32_WINNT 0x0A00
//#define BOOST_DATE_TIME_NO_LIB
//#define BOOST_REGEX_NO_LIB

//#define _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING

//i userd this https://stackoverflow.com/questions/53861300/how-do-you-properly-install-libcurl-for-use-in-visual-studio-2017
//THANK YOU!!111!11!!

#include "cpr/cpr.h"

using namespace std;

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

        for (auto word : argsplus)
        {
            if (eqleft != word) continue;
            str += ('+' + eqleft + ' ');
            if (line.find("=TRUE") == STRINGNPOS)
            {
                const auto eqright = line.substr(equalpos + 1);
                if (word == "sv_setsteamaccount")
                {
                    if (eqright.length() != 32)
                    {
                        cout << "wrong sv_setsteamaccount\n";
                        system("pause");
                        throw exception("wrong sv_setsteamaccount");
                    }
                }
                str += (eqright + ' ');
            }
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


char bad_responses = 0;
void CheckServerInOnline()
{
    this_thread::sleep_for(std::chrono::seconds(60));

    const auto myip = cpr::Get(cpr::Url{ "https://ipv4bot.whatismyipaddress.com/" }).text;
    if (myip.length() < 7) return;

    const string fulladdr = myip + ":" + port;

    const auto str = cpr::Get(cpr::Url{ "https://api.steampowered.com/ISteamApps/GetServersAtAddress/v1/?addr=" + fulladdr }).text;

 
    if (str.find(fulladdr) == STRINGNPOS)
    {
        bad_responses++;
        if (bad_responses == 5)
        {
            const string command = string("taskkill /F /FI ") + '"' + "WINDOWTITLE eq " + windowtitle1 + '"' + " /T";
            system("echo (%date% %time%) server didnt rersponse for 5 times. Restarting...");
            system(command.c_str());
            system("start start.bat");
            bad_responses = 0;
        }
    }
    else bad_responses = 0;

}

int main(int argc, char** argv)
{
    bool notprogramend = true;

    StartServer(argv);

    thread t([&notprogramend]() {while (notprogramend) CheckServerInOnline();});
    t.detach();

    while (notprogramend)
    {
        char s = getchar();
        if (s != 's') continue;

        notprogramend = false;
        t.~thread();
        const string command = string("taskkill /F /FI ") + '"' + "WINDOWTITLE eq " + windowtitle1 + '"' + " /T";
        system("echo Stopping server...");
        system(command.c_str());
        system("pause");
    }

    return 0;
}