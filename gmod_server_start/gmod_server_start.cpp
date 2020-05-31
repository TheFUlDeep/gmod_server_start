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

void DeleteSpacesInLine(string& line)
{
    while (line.front() == ' ' || line.front() == '\t') line.erase(0,1);

    while (line.back() == ' ' || line.back() == '\t') line.pop_back();
}

void RemoveSymbolFromString(string &str, const char &symbol)
{
    auto pos = str.find(symbol);
    while (pos != STRINGNPOS)
    {
        str.erase(pos, 1);
        pos = str.find(symbol);
    }
}

void AddArgs(const string &line, const string &eqleft, const size_t &equalpos, const char *word, string &str, bool isplus = false)
{
    if (eqleft != word) return;
    auto eqright = line.substr(equalpos + 1); DeleteSpacesInLine(eqright);
    if (eqright == "FALSE" || eqright.find(' ') != STRINGNPOS || eqright.find('\t') != STRINGNPOS) return;

    char plus = '+';
    if (!isplus) plus = '-';
    str += (plus + eqleft + ' ');
    if (eqright != "TRUE")
    {
        if (isplus && string(word) == "sv_setsteamaccount")//word == "sv_setsteamaccount уже подразумевает, что isplus == true, но пускай вначале быстро проверится бул, а потом уже стринг
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

const string GetStartArgs()
{
    string str = "";

    auto linesstack = FileLinesToStack("server\\startargs.txt");

    while (!linesstack.empty())
    {
        auto line = linesstack.top();
        DeleteSpacesInLine(line);
        linesstack.pop();

        //пропускаю закомментированные строки или неправильные строки (в которых нет знака =) или FALSE
        if (line.front() == '#' || line.find('+') != STRINGNPOS || line.find('-') != STRINGNPOS) continue;
        const auto equalpos = line.find('=');
        if (equalpos == STRINGNPOS) continue;

        //если в правой части есть + или -, то игнорить эту строку (для безопасности, чтобы нельзя было дописать свои аргументы)
        //возможно лучше делать проверку на + и - с пробелом, но если ни в одной команде нет ни + не -, то проще не менять

        auto eqleft = line.substr(0, equalpos); DeleteSpacesInLine(eqleft);
        if (eqleft.find(' ') != STRINGNPOS || eqleft.find('\t') != STRINGNPOS) continue;

        for (auto word : argsminus) AddArgs(line, eqleft, equalpos, word, str);

        for (auto word : argsplus) AddArgs(line, eqleft, equalpos, word, str, true);

    }
    
    str.pop_back();
    return str;
}

string port = "";
string windowtitle1 = "";
string processkillcommand = "";
void StartServer()
{
    auto adminargsstack = FileLinesToStack("adminargs.txt");

    auto additionalargs = adminargsstack.top(); adminargsstack.pop(); DeleteSpacesInLine(additionalargs);

    auto tickrate = adminargsstack.top(); adminargsstack.pop(); DeleteSpacesInLine(additionalargs);

    port = adminargsstack.top(); adminargsstack.pop(); DeleteSpacesInLine(port);
    /*try
    {
        port = to_string(stoi(port));
    }
    catch (const exception& e)
    {
        system("echo wrong port");
        system("pause");
        throw exception("wrong port");
    }*/

    auto name = adminargsstack.top(); adminargsstack.pop(); DeleteSpacesInLine(name);

    windowtitle1 = name + " server watchdog";

    processkillcommand = string("taskkill /F /FI ") + '"' + "WINDOWTITLE eq " + windowtitle1 + '"' + " /T";

    const string start = ("@echo off\ncls\ncd ./server\necho Protecting srcds from crashes...\ntitle " + windowtitle1 + "\n:srcds\necho (%date% %time%) srcds started.\n");
    const string prestartargs = (start + "start /wait " + additionalargs + " srcds.exe -nocrashdialog -console -tickrate " + tickrate + " -autoupdate -game garrysmod -port " + port + " ");
    const string end = "\necho (%date% %time%) WARNING: srcds closed or crashed, restarting.\ngoto srcds";

    const auto startargs = GetStartArgs();

    const auto batcommand = (prestartargs + startargs + end);


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
            system("echo (%date% %time%) server didnt rersponse for 5 times. Restarting...");
            system(processkillcommand.c_str());
            system("start start.bat");
            bad_responses = 0;
        }
    }
    else bad_responses = 0;

}

int main()
{
    bool notprogramend = true;

    StartServer();

    thread t([&notprogramend]() {while (notprogramend) CheckServerInOnline();});
    t.detach();

    while (notprogramend)
    {
        char s = getchar();
        if (s == 's')
        {
            notprogramend = false;
            t.~thread();
            system("echo (%date% %time%) Stopping server...");
            system(processkillcommand.c_str());
            system("pause");
        }
        else if (s == 'r')
        {
            system("echo (%date% %time%) Restarting server...");
            system(processkillcommand.c_str());
            bad_responses = 0;
            system("start start.bat");
        }
    }

    return 0;
}